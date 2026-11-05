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
    buffer.clear();      // Milestone 1 makes no sound of its own: your DAW's instruments do
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
    for (const auto& e : events)
        midi.addEvent (juce::MidiMessage (e.status, e.d1, e.d2), e.offset);

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
