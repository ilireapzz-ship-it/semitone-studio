#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SongFiles.h"

SemitoneProcessor::SemitoneProcessor()
    : AudioProcessor (juce::JUCEApplicationBase::isStandaloneApp()
                          ? BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), false).withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          : BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    events.reserve (4096);
    // standalone app: come back to the song you were working on, even after a crash
    if (isStandalone())
    {
        host = std::make_unique<st::PartHost>();
        const auto f = st::files::autosaveFile();
        if (f.existsAsFile()) projectJson = f.loadFileAsString();
    }
}

void SemitoneProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    const bool changed = sampleRate != sr;
    sr = sampleRate;
    if (changed) synth.setPatch (nullptr);   // notes rendered for the old rate would play out of tune until the rebuild lands
    synth.prepare (sr);
    if (host) host->prepare (sr, maxBlock);
    if (changed) buildSynthPatch();   // the piano and plucked notes are rendered for one sample rate
}

bool SemitoneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::disabled();
}

void SemitoneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (takeRecorder.active() && getTotalNumInputChannels() > 0 && buffer.getNumChannels() > 0)
        takeRecorder.push (buffer.getReadPointer (0), getTotalNumInputChannels() > 1 && buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : nullptr, buffer.getNumSamples());
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
    if (mode == 0 || mode == 2)
        for (const auto& e : events)
            if (! ((midiMute.load() >> (e.status & 15)) & 1))
                midi.addEvent (juce::MidiMessage (e.status, e.d1, e.d2), e.offset);

    // Semitone's own instruments, rendered by the interface, played on the DAW's timeline
    // (the standalone app plays its sound in the interface itself)
    // In the app the page plays too, on its own clock. With a plug-in on a part the engine plays everything instead,
    // so drums and plug-ins share this one clock and land on the same sample.
    if (mode != 2 && (! isStandalone() || engineAll.load()) && buffer.getNumChannels() > 0)
    {
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
        const double len = tlLength.load(), off = tlOffset.load();
        // rendered sections and takes are audio: played at another tempo they change pitch, so then the engine takes over
        const double songTempo = songBpm.load();
        const bool tempoMatches = isStandalone() || songTempo <= 0 || std::abs (bpm - songTempo) < .5;
        if (mode != 3 && tempoMatches) clipPlayer.process (playing, ppq, bpm, sr, n, len, off, L, R);
        // the native engine fills in wherever no rendered section covers the playhead
        bool covered = false;
        if (mode != 3 && len > 0 && tempoMatches)
        {
            double pos = std::fmod (ppq, len);
            if (pos < 0) pos += len;
            covered = clipPlayer.covers (pos + off);
        }
        double beat = -1;
        if (playing && len > 0)
        {
            double pos = std::fmod (ppq, len);
            if (pos < 0) pos += len;
            beat = pos + off;
        }
        synth.takeLevel = tempoMatches ? takeGain.load() : 0.0f;
        synth.process (events, n, L, R, covered ? 0.0f : 1.0f, beat, bpm / 60.0 / sr);
    }
    else if (mode == 2) synth.silence();

    // your own plug-ins on Semitone's parts (desktop app)
    if (host && buffer.getNumChannels() > 0)
    {
        const double loop = tlLength.load();
        double songBeat = -1;
        if (playing && loop > 0) { songBeat = std::fmod (ppq, loop); if (songBeat < 0) songBeat += loop; songBeat += tlOffset.load(); }
        host->process (events, n, buffer.getWritePointer (0), buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr, songBeat);
    }

    uiPlaying = playing; uiPpq = ppq; uiBpm = bpm; uiHostHasTransport = hostTransport;
}

void SemitoneProcessor::setTimelineFromVar (const juce::var& v)
{
    // { length: beats, notes: [start, dur, channel, key, velocity, start, dur, ...] }
    auto tl = std::make_shared<st::Timeline>();
    tl->length = (double) v.getProperty ("length", 0.0);
    songBpm = (double) v.getProperty ("bpm", 0.0);
    engineAll = (bool) v.getProperty ("engineAll", false);
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
    setSynthSound (v.getProperty ("sound", juce::var()), *tl);
    const juce::ScopedLock sl (stateLock);
    timelineJson = juce::JSON::toString (v, true);
}

// ---- the native engine's patch: the page sends the settings of every part (nativeSoundInfo in the HTML) ----
static float num (const juce::var& o, const char* key, float fallback)
{
    const auto v = o.getProperty (key, juce::var());
    return v.isVoid() ? fallback : (float) (double) v;
}

static st::Wave waveFrom (const juce::String& t)
{
    if (t == "sine") return st::Wave::sine;
    if (t == "triangle" || t == "tri") return st::Wave::triangle;
    if (t == "square") return st::Wave::square;
    if (t == "pulse") return st::Wave::pulse;
    return st::Wave::saw;
}

template <size_t N>
static void numsInto (const juce::var& o, const char* key, std::array<float, N>& dest)
{
    if (auto* a = o.getProperty (key, juce::var()).getArray())
        for (size_t i = 0; i < N && (int) i < a->size(); ++i) dest[i] = (float) (double) (*a)[(int) i];
}

static st::PartPatch partFrom (const juce::var& o)
{
    st::PartPatch p;
    p.ch = juce::jlimit (0, 15, (int) o.getProperty ("ch", 0));
    p.vol = num (o, "vol", .2f); p.velRef = juce::jmax (1.0f, num (o, "velRef", 92)); p.level = num (o, "level", 1); p.pan = num (o, "pan", 0);
    const auto kind = o.getProperty ("kind", "voice").toString();
    if (kind == "fm") p.kind = st::Kind::fm;
    else if (kind == "piano" || kind == "pluck")
    {
        p.kind = st::Kind::buffer;
        p.bufRelease = num (o, "rel", .22f); p.holdMin = num (o, "holdMin", 0); p.holdMax = num (o, "holdMax", 0);
        p.velCutoff = kind == "piano";
        const auto id = o.getProperty ("instr", "").toString();
        p.instr = kind == "piano" ? 0 : 1 + (int) (id == "steel" ? st::Pluck::steel : id == "harp" ? st::Pluck::harp : id == "pizz" ? st::Pluck::pizz : id == "bassgtr" ? st::Pluck::bassgtr : id == "upright" ? st::Pluck::upright : st::Pluck::nylon);
    }
    else if (kind == "juno" || kind == "pro" || kind == "prophet")
    {
        p.kind = kind == "juno" ? st::Kind::juno : st::Kind::prophet;
        p.saw = num (o, "saw", 0); p.pulse = num (o, "pulse", 0); p.pw = num (o, "pw", .5f); p.pwm = num (o, "pwm", 0); p.sub = num (o, "sub", 0); p.noise = num (o, "noise", 0);
        p.cut = num (o, "cut", 1000); p.res = num (o, "res", .1f); p.env = num (o, "env", 1); p.kt = num (o, "kt", .5f); p.drift = num (o, "drift", .3f); p.gain = num (o, "gain", 1); p.scoop = num (o, "scoop", 0);
        p.det = num (o, "det", 0); p.pm = num (o, "pm", 0);
        const auto chorus = o.getProperty ("chorus", "").toString();
        p.chorus = chorus == "I" ? 1 : chorus == "II" ? 2 : chorus == "I+II" ? 3 : 0;
        p.bMul = num (o, "bRatio", 1) * std::pow (2.0f, num (o, "bOct", 0));
        p.hpHz = p.kind == st::Kind::juno ? ((bool) o.getProperty ("mono", false) ? 30.0f : 90.0f) : 40.0f;
        numsInto (o, "fe", p.fe); numsInto (o, "ae", p.ae); numsInto (o, "lfo", p.lfo);
        if (auto* a = o.getProperty ("a", juce::var()).getArray()) if (a->size() >= 2)
        { p.aWave = waveFrom ((*a)[0].toString()); p.aGain = (float) (double) (*a)[1]; if (a->size() > 2) p.aPw = (float) (double) (*a)[2]; }
        if (auto* b = o.getProperty ("b", juce::var()).getArray()) if (b->size() >= 2)
        { p.bWave = waveFrom ((*b)[0].toString()); p.bGain = (float) (double) (*b)[1]; if (b->size() > 2) p.bPw = (float) (double) (*b)[2]; }
    }
    else
    {
        p.kind = st::Kind::voice;
        p.wave = waveFrom (o.getProperty ("type", "triangle").toString());
        p.attack = num (o, "attack", .01f); p.release = num (o, "release", .3f); p.cutoff = num (o, "cutoff", 4000); p.decay = num (o, "decay", 0);
        p.unison = num (o, "unison", 0); p.fenv = num (o, "fenv", 0); p.vib = num (o, "vib", 0); p.vibRate = num (o, "vibRate", 5.6f);
        if (auto* ls = o.getProperty ("layers", juce::var()).getArray())
            for (const auto& l : *ls)
                if (auto* a = l.getArray(); a != nullptr && a->size() >= 3 && p.numLayers < (int) p.layers.size())
                    p.layers[(size_t) p.numLayers++] = { (float) (double) (*a)[0], (float) (double) (*a)[2], waveFrom ((*a)[1].toString()) };
    }
    p.stereo = (bool) o.getProperty ("stereo", false);
    return p;
}

void SemitoneProcessor::setSynthSound (const juce::var& sound, const st::Timeline& tl)
{
    std::shared_ptr<st::SynthPatch> t;
    std::vector<std::set<int>> keys;
    if (sound.isObject())   // none for the orchestral view, and for projects saved before this engine existed
    {
        t = std::make_shared<st::SynthPatch>();
        if (auto* parts = sound.getProperty ("parts", juce::var()).getArray())
            for (const auto& o : *parts)
            {
                t->parts.push_back (partFrom (o));
                std::set<int> used;
                for (const auto& n : tl.notes) if (n.ch == t->parts.back().ch) used.insert (n.key);
                keys.push_back (std::move (used));
            }
        const auto d = sound.getProperty ("drums", juce::var());
        if (d.isObject())
        {
            t->drumCh = juce::jlimit (0, 15, (int) d.getProperty ("ch", 9));
            t->drumLevel = num (d, "level", 1); t->drumPan = num (d, "pan", 0);
            const auto k = d.getProperty ("kit", juce::var());
            t->kit.lp = num (k, "lp", 16000); t->kit.clap = num (k, "clap", .22f);
            numsInto (k, "kick", t->kit.kick); numsInto (k, "snare", t->kit.snare); numsInto (k, "hat", t->kit.hat);
            numsInto (k, "open", t->kit.open); numsInto (k, "rim", t->kit.rim); numsInto (k, "crash", t->kit.crash);
        }
        t->bendRange = (float) juce::jlimit (1, 48, tl.bendRange > 0 ? tl.bendRange : 12);
        if (auto* lanes = sound.getProperty ("lanes", juce::var()).getArray())
            for (const auto& o : *lanes)
            {
                st::Lane k;
                k.ch = (int) o.getProperty ("ch", -1); k.pan = (bool) o.getProperty ("pan", false);
                if (auto* p = o.getProperty ("pts", juce::var()).getArray())
                    for (int i = 0; i + 1 < p->size(); i += 2) k.pts.push_back ({ (double) (*p)[i], (float) (double) (*p)[i + 1] });
                t->lanes.push_back (std::move (k));
            }
        if (auto* fx = sound.getProperty ("fx", juce::var()).getArray())
        {
            t->fxList = std::make_shared<std::vector<st::FxDesc>>();
            for (const auto& o : *fx)
            {
                st::FxDesc d;
                d.ch = (int) o.getProperty ("ch", 0); d.slot = (int) o.getProperty ("slot", 0);
                d.type = o.getProperty ("type", "").toString().toStdString(); d.style = o.getProperty ("style", "").toString().toStdString();
                if (auto* obj = o.getProperty ("p", juce::var()).getDynamicObject())
                    for (const auto& kv : obj->getProperties()) if (kv.value.isDouble() || kv.value.isInt() || kv.value.isInt64()) d.p[kv.name.toString().toStdString()] = (float) (double) kv.value;
                t->fxList->push_back (std::move (d));
            }
        }
        if (auto* list = sound.getProperty ("trans", juce::var()).getArray())
        {
            t->trans = std::make_shared<std::vector<st::TransEv>>();
            for (const auto& o : *list)
            {
                st::TransEv e;
                const auto k = o.getProperty ("k", "").toString();
                e.kind = k == "riser" ? st::TransEv::riser : k == "impact" ? st::TransEv::impact : k == "sweep" ? st::TransEv::sweep : k == "snare" ? st::TransEv::snare : st::TransEv::noise;
                e.at = (double) o.getProperty ("at", 0.0); e.dur = num (o, "dur", 1); e.f0 = num (o, "f0", 1000); e.f1 = num (o, "f1", 1000); e.g0 = num (o, "g0", .1f); e.g1 = num (o, "g1", .1f); e.q = num (o, "q", 1);
                const auto fl = o.getProperty ("filter", "bandpass").toString();
                e.filter = fl == "highpass" ? 1 : fl == "lowpass" ? 2 : 0;
                t->trans->push_back (e);
            }
            std::stable_sort (t->trans->begin(), t->trans->end(), [] (const st::TransEv& a, const st::TransEv& b) { return a.at < b.at; });
            t->transLevel = num (sound, "transLevel", 1);
            if (t->trans->empty()) t->trans.reset();
        }
        if (auto* list = sound.getProperty ("takes", juce::var()).getArray())
        {
            t->takes = std::make_shared<std::vector<st::TakeSeg>>();
            t->takeFiles.clear();
            for (const auto& o : *list)
            {
                st::TakeSeg g; g.beat = (double) o.getProperty ("beat", 0.0); g.end = g.beat + (double) o.getProperty ("beats", 0.0); g.start = (double) o.getProperty ("start", 0.0);
                t->takes->push_back (g); t->takeFiles.push_back (o.getProperty ("file", "").toString().toStdString());
            }
            if (t->takes->empty()) t->takes.reset();
        }
        takeGain = num (sound, "takeLevel", 1);
        if (auto* lanes = sound.getProperty ("fxLanes", juce::var()).getArray())
            for (const auto& o : *lanes)
            {
                st::Lane k;
                k.ch = (int) o.getProperty ("ch", -1); k.fxSlot = (int) o.getProperty ("slot", 0); k.fxName = o.getProperty ("k", "").toString().toStdString();
                if (auto* p = o.getProperty ("pts", juce::var()).getArray())
                    for (int i = 0; i + 1 < p->size(); i += 2) k.pts.push_back ({ (double) (*p)[i], (float) (double) (*p)[i + 1] });
                t->lanes.push_back (std::move (k));
            }
        if (host)
        {
            auto list = std::make_shared<std::vector<st::Lane>>();
            if (auto* pl = sound.getProperty ("plugLanes", juce::var()).getArray())
                for (const auto& o : *pl)
                {
                    st::Lane k;
                    k.ch = (int) o.getProperty ("ch", 0); k.fxParam = (int) o.getProperty ("i", -1);
                    if (auto* p = o.getProperty ("pts", juce::var()).getArray())
                        for (int i = 0; i + 1 < p->size(); i += 2) k.pts.push_back ({ (double) (*p)[i], (float) (double) (*p)[i + 1] });
                    list->push_back (std::move (k));
                }
            host->setLanes (list);
        }
        uint32_t mute = 0;
        if (auto* m = sound.getProperty ("midiMute", juce::var()).getArray())
            for (const auto& c : *m) mute |= 1u << juce::jlimit (0, 15, (int) c);
        midiMute = mute;
        const auto r = sound.getProperty ("room", juce::var());
        if (r.isObject())
        {
            t->roomMix = num (r, "mix", 0);
            // the page's room rings as (1 - x)^decay across its length: this is where that reaches -60 dB
            t->roomT60 = num (r, "len", 1) * (1 - std::pow (10.0f, -3 / juce::jmax (.1f, num (r, "decay", 3))));
        }
        const auto pl = sound.getProperty ("polish", juce::var());
        if (pl.isObject())
        {
            auto& q = t->polish;
            q.on = true; q.hp = num (pl, "hp", 28); q.thr = num (pl, "thr", -18); q.ratio = num (pl, "ratio", 2); q.att = num (pl, "att", .03f); q.rel = num (pl, "rel", .25f);
            q.shelfLo = num (pl, "shelfLo", 0); q.shelfHi = num (pl, "shelfHi", 0); q.make = num (pl, "make", 1); q.lim = num (pl, "lim", -2); q.drive = num (pl, "drive", .8f);
        }
    }
    {
        const juce::ScopedLock sl (synthLock);
        synthTemplate = t;
        synthKeys = std::move (keys);
    }
    buildSynthPatch();
}

// Renders the piano and plucked notes the song needs (once each), away from the audio thread, then swaps the patch in.
void SemitoneProcessor::buildSynthPatch()
{
    const int gen = ++synthGen;
    const double rate = sr;
    synthPool.addJob ([this, gen, rate]
    {
        std::shared_ptr<const st::SynthPatch> tmpl;
        std::vector<std::set<int>> keys;
        {
            const juce::ScopedLock sl (synthLock);
            tmpl = synthTemplate;
            keys = synthKeys;
        }
        std::shared_ptr<st::SynthPatch> built;
        if (tmpl != nullptr)
        {
            built = std::make_shared<st::SynthPatch> (*tmpl);
            built->buffers.resize (built->parts.size());
            if (built->takes)
            {
                auto segs = std::make_shared<std::vector<st::TakeSeg>> (*built->takes);
                for (size_t i = 0; i < segs->size() && i < built->takeFiles.size(); ++i)
                {
                    const auto& name = built->takeFiles[i];
                    auto& slot = takeCache[name];
                    if (slot == nullptr) slot = st::readTake (st::files::takesFolder().getChildFile (juce::String (name)).getFullPathName().toStdString());
                    (*segs)[i].audio = slot;
                }
                built->takes = segs;
            }
            if (built->fxList)
            {
                built->fx = st::makeFxBank (*built->fxList, rate);
                for (auto& k : built->lanes)
                    if (k.fxSlot >= 0)
                        if (auto* f = built->fx->at (k.ch < 0 ? st::FxBank::master : k.ch, k.fxSlot)) k.fxParam = f->param (k.fxName);
                built->lanes.erase (std::remove_if (built->lanes.begin(), built->lanes.end(), [] (const st::Lane& k) { return k.fxSlot >= 0 && k.fxParam < 0; }), built->lanes.end());
            }
            for (size_t i = 0; i < built->parts.size(); ++i)
            {
                auto& nb = built->buffers[i];
                nb.fill (nullptr);
                const auto& p = built->parts[i];
                if (p.instr < 0 || i >= keys.size()) continue;
                for (int key : keys[i])
                {
                    if (synthGen.load() != gen) return;   // a newer song arrived: don't finish this one
                    const auto id = ((juce::int64) (int) rate << 16) | ((juce::int64) p.instr << 8) | key;
                    auto& slot = noteCache[id];
                    if (slot == nullptr)
                        slot = std::make_unique<st::NoteBuffer> (p.instr == 0 ? st::renderPianoNote (key, rate) : st::renderPluckNote (key, rate, (st::Pluck) (p.instr - 1)));
                    nb[(size_t) key] = slot.get();
                }
            }
        }
        // the audio thread may still hold the previous patch: keep it a little while
        const auto now = juce::Time::getMillisecondCounter();
        if (patchNow != nullptr) retiredPatches.push_back ({ patchNow, now });
        retiredPatches.erase (std::remove_if (retiredPatches.begin(), retiredPatches.end(), [now] (const auto& r) { return now - r.second > 5000; }), retiredPatches.end());
        patchNow = built;
        synth.setPatch (patchNow);
    });
}

void SemitoneProcessor::setStandaloneTransport (bool play, double startBeat, double bpm)
{
    if (bpm > 0) stBpm = bpm;
    if (play) { stStartRequest = juce::jmax (0.0, startBeat); sequencer.chaseOnNextBlock(); }
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
