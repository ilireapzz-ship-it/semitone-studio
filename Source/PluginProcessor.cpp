#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SongFiles.h"

SemitoneProcessor::SemitoneProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    events.reserve (4096);
    // standalone app: come back to the song you were working on, even after a crash
    if (isStandalone())
    {
        const auto f = st::files::autosaveFile();
        if (f.existsAsFile()) projectJson = f.loadFileAsString();
    }
}

bool SemitoneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::disabled();
}

void SemitoneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    // pass notes you play to the interface (it uses them for play-along and live recording)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOnOrOff() || m.isSustainPedalOn() || m.isSustainPedalOff())
        {
            const auto* d = m.getRawData();
            const auto packed = (juce::uint32) d[0] | ((juce::uint32) d[1] << 8) | ((juce::uint32) (m.getRawDataSize() > 2 ? d[2] : 0) << 16);
            const auto w = inFifo.write (1);
            if (w.blockSize1 > 0) inBuf[(size_t) w.startIndex1] = packed;
            else if (w.blockSize2 > 0) inBuf[(size_t) w.startIndex2] = packed;
        }
    }
    midi.clear();        // incoming MIDI is not passed through
    const int n = buffer.getNumSamples();

    bool playing = false; double ppq = 0.0, bpm = 120.0; bool hostTransport = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) { ppq = *p; hostTransport = true; }
            playing = pos->getIsPlaying();
        }
    }

    if (isStandalone() || ! hostTransport)
    {
        // our own transport (standalone app, or a host that gives no position)
        const double req = stStartRequest.exchange (-1.0);
        if (req >= 0.0) stPpq = req;
        playing = stPlaying.load();
        bpm = stBpm.load();
        ppq = stPpq;
        if (playing) stPpq += n * (bpm / 60.0) / sr;
        hostTransport = false;
    }

    events.clear();
    sequencer.process (playing, ppq, bpm, sr, n, events);
    const int mode = soundMode.load();
    if (mode != 1)
        for (const auto& e : events)
            midi.addEvent (juce::MidiMessage (e.status, e.d1, e.d2), e.offset);

    // Semitone's own instruments, rendered by the interface, played on the DAW's timeline
    // (the standalone app plays its sound in the interface itself)
    if (mode != 2 && ! isStandalone() && buffer.getNumChannels() > 0)
    {
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
        clipPlayer.process (playing, ppq, bpm, sr, n, tlLength.load(), tlOffset.load(), L, R);
    }

    uiPlaying = playing; uiPpq = ppq; uiBpm = bpm; uiHostHasTransport = hostTransport;
}

void SemitoneProcessor::setTimelineFromVar (const juce::var& v)
{
    // { length: beats, notes: [start, dur, channel, key, velocity, start, dur, ...] }
    auto tl = std::make_shared<st::Timeline>();
    tl->length = (double) v.getProperty ("length", 0.0);
    if (auto* arr = v.getProperty ("notes", juce::var()).getArray())
    {
        const int count = arr->size() / 5;
        tl->notes.reserve ((size_t) count);
        for (int i = 0; i < count; ++i)
        {
            st::Note nt;
            nt.start = (double) (*arr)[i * 5];
            nt.end = nt.start + (double) (*arr)[i * 5 + 1];
            nt.ch = juce::jlimit (0, 15, (int) (*arr)[i * 5 + 2]);
            nt.key = juce::jlimit (0, 127, (int) (*arr)[i * 5 + 3]);
            nt.vel = juce::jlimit (1, 127, (int) (*arr)[i * 5 + 4]);
            tl->notes.push_back (nt);
        }
    }
    tl->bendRange = (int) v.getProperty ("bendRange", 0);
    tlLength = tl->length;
    tlOffset = (double) v.getProperty ("offset", 0.0);
    if (auto* ev = v.getProperty ("ev", juce::var()).getArray())
    {
        const int count = ev->size() / 4;
        tl->ctls.reserve ((size_t) count);
        for (int i = 0; i < count; ++i)
        {
            st::Ctl c;
            c.at = (double) (*ev)[i * 4];
            c.status = (unsigned char) juce::jlimit (0x80, 0xEF, (int) (*ev)[i * 4 + 1]);
            c.d1 = (unsigned char) juce::jlimit (0, 127, (int) (*ev)[i * 4 + 2]);
            c.d2 = (unsigned char) juce::jlimit (0, 127, (int) (*ev)[i * 4 + 3]);
            tl->ctls.push_back (c);
        }
    }
    tl->sort();
    sequencer.setTimeline (tl);
    const juce::ScopedLock sl (stateLock);
    timelineJson = juce::JSON::toString (v, true);
}

void SemitoneProcessor::setStandaloneTransport (bool play, double startBeat, double bpm)
{
    if (bpm > 0) stBpm = bpm;
    if (play) stStartRequest = juce::jmax (0.0, startBeat);
    stPlaying = play;
}

void SemitoneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    // The whole Semitone song is saved inside the DAW project.
    juce::ValueTree t ("SemitoneState");
    { const juce::ScopedLock sl (stateLock); t.setProperty ("project", projectJson, nullptr); t.setProperty ("timeline", timelineJson, nullptr); t.setProperty ("songFile", songFile, nullptr); }
    t.setProperty ("soundMode", soundMode.load(), nullptr);
    t.setProperty ("audioLayout", juce::JSON::toString (audioLayout, true), nullptr);
    t.setProperty ("version", 1, nullptr);
    juce::MemoryOutputStream mo (dest, false);
    t.writeToStream (mo);
}

void SemitoneProcessor::setStateInformation (const void* data, int size)
{
    auto t = juce::ValueTree::readFromData (data, (size_t) size);
    if (! t.isValid()) return;
    juce::String tl;
    {
        const juce::ScopedLock sl (stateLock);
        // the standalone app's autosave is always newer than the state saved when it last quit
        if (! (isStandalone() && projectJson.isNotEmpty())) projectJson = t.getProperty ("project").toString();
        tl = t.getProperty ("timeline").toString();
        if (t.hasProperty ("songFile")) songFile = t.getProperty ("songFile").toString();
    }
    if (tl.isNotEmpty()) setTimelineFromVar (juce::JSON::parse (tl)); // plays even before the window is opened
    if (t.hasProperty ("soundMode")) setSoundMode ((int) t.getProperty ("soundMode"));
    if (t.hasProperty ("audioLayout")) setAudioLayout (juce::JSON::parse (t.getProperty ("audioLayout").toString())); // sound from the clip cache
}

// ---- Semitone's own sounds ----
// Clips are cached on disk by id (a hash of everything that shapes the section's sound),
// so a reopened DAW project plays at once, without opening the Semitone window.
static juce::File clipCacheDir()
{
    auto d = st::files::appData().getChildFile ("AudioCache");
    d.createDirectory();
    return d;
}

static juce::String safeId (const juce::String& id) { return id.retainCharacters ("0123456789abcdefABCDEF-_").substring (0, 40); }

std::shared_ptr<st::Clip> SemitoneProcessor::loadCachedClip (const juce::String& id)
{
    const auto f = clipCacheDir().getChildFile (safeId (id) + ".stclip");
    juce::FileInputStream in (f);
    if (! in.openedOk() || in.readInt() != 0x53544331) return nullptr; // "STC1"
    auto c = std::make_shared<st::Clip>();
    c->sr = in.readInt(); c->frames = in.readInt();
    const int nm = in.readInt();
    if (c->sr < 8000 || c->sr > 384000 || c->frames < 2 || c->frames > 384000 * 900 || nm < 0 || nm > 100000) return nullptr;
    for (int i = 0; i < nm; ++i) { c->mb.push_back (in.readDouble()); c->ms.push_back (in.readDouble()); }
    c->pcm.resize ((size_t) c->frames * 2);
    const auto bytes = (int) (c->pcm.size() * sizeof (int16_t));
    if (in.read (c->pcm.data(), bytes) != bytes) return nullptr;
    f.setLastAccessTime (juce::Time::getCurrentTime());
    return c;
}

static void saveCachedClip (const juce::String& id, const st::Clip& c)
{
    const auto f = clipCacheDir().getChildFile (safeId (id) + ".stclip");
    juce::TemporaryFile tmp (f);
    {
        juce::FileOutputStream out (tmp.getFile());
        if (! out.openedOk()) return;
        out.writeInt (0x53544331); out.writeInt (c.sr); out.writeInt (c.frames); out.writeInt ((int) c.mb.size());
        for (size_t i = 0; i < c.mb.size(); ++i) { out.writeDouble (c.mb[i]); out.writeDouble (c.ms[i]); }
        out.write (c.pcm.data(), c.pcm.size() * sizeof (int16_t));
        out.flush();
        if (out.getStatus().failed()) return;
    }
    tmp.overwriteTargetFileWithTemporary();
}

static void pruneClipCache()
{
    static bool done = false;
    if (done) return;
    done = true;
    const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::days (45);
    for (const auto& f : clipCacheDir().findChildFiles (juce::File::findFiles, false, "*.stclip"))
        if (f.getLastModificationTime() < cutoff && f.getLastAccessTime() < cutoff) f.deleteFile();
}

juce::StringArray SemitoneProcessor::setAudioLayout (const juce::var& items)
{
    pruneClipCache();
    audioLayout = items.isArray() ? items : juce::var (juce::Array<juce::var>());
    juce::StringArray need;
    if (auto* arr = audioLayout.getArray())
        for (const auto& it : *arr)
        {
            const auto id = it.getProperty ("id", "").toString();
            if (id.isEmpty() || clips.count (id) || need.contains (id)) continue;
            if (auto c = loadCachedClip (id)) clips[id] = c;
            else need.add (id);
        }
    rebuildBank();
    return need;
}

void SemitoneProcessor::addClip (const juce::String& id, std::shared_ptr<st::Clip> clip)
{
    if (! clip || id.isEmpty()) return;
    saveCachedClip (id, *clip);
    clips[id] = std::move (clip);
    rebuildBank();
}

void SemitoneProcessor::rebuildBank()
{
    auto b = std::make_shared<st::Bank>();
    juce::StringArray used;
    if (auto* arr = audioLayout.getArray())
        for (const auto& it : *arr)
        {
            const auto id = it.getProperty ("id", "").toString();
            auto f = clips.find (id);
            if (f == clips.end()) continue;
            b->items.push_back ({ f->second, (double) it.getProperty ("at", 0.0) });
            used.add (id);
        }
    // forget clips the song no longer uses (they stay in the disk cache)
    for (auto it = clips.begin(); it != clips.end();)
        it = used.contains (it->first) ? std::next (it) : clips.erase (it);
    // the audio thread may still hold the previous bank: keep it alive a little while
    if (auto old = std::atomic_load (&bankNow)) retired.push_back ({ old, juce::Time::getMillisecondCounter() });
    std::shared_ptr<const st::Bank> cb = b;
    std::atomic_store (&bankNow, cb);
    clipPlayer.setBank (cb);
}

void SemitoneProcessor::releaseRetired()
{
    const auto now = juce::Time::getMillisecondCounter();
    retired.erase (std::remove_if (retired.begin(), retired.end(), [now] (const auto& r) { return now - r.second > 5000; }), retired.end());
}

int SemitoneProcessor::popIncomingMidi (std::vector<juce::uint32>& dest)
{
    const auto r = inFifo.read (inFifo.getNumReady());
    for (int i = 0; i < r.blockSize1; ++i) dest.push_back (inBuf[(size_t) (r.startIndex1 + i)]);
    for (int i = 0; i < r.blockSize2; ++i) dest.push_back (inBuf[(size_t) (r.startIndex2 + i)]);
    return r.blockSize1 + r.blockSize2;
}

juce::AudioProcessorEditor* SemitoneProcessor::createEditor() { return new SemitoneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SemitoneProcessor(); }
