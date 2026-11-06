#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Sequencer.h"
#include "Synth.h"
#include "SongFiles.h"

// Phase 3, desktop app only: your own instrument plug-ins on Semitone's parts. Each MIDI channel can hold one.
// Finding plug-ins opens each one, so it runs a file at a time on request, and a plug-in that crashes is skipped next time.
namespace st
{
class HostWindow : public juce::DocumentWindow
{
public:
    HostWindow (juce::AudioPluginInstance& p, std::function<void()> closed)
        : DocumentWindow (p.getName(), juce::Colour (0xff15191e), closeButton), onClosed (std::move (closed))
    {
        setUsingNativeTitleBar (true);
        if (auto* ed = p.createEditorIfNeeded()) setContentOwned (ed, true);
        centreWithSize (juce::jmax (300, getWidth()), juce::jmax (200, getHeight()));
        setVisible (true);
    }
    void closeButtonPressed() override { if (onClosed) onClosed(); }
private:
    std::function<void()> onClosed;
};

class PartHost : private juce::Timer
{
public:
    std::function<void()> onList;                                   // the plug-in list changed
    std::function<void (int, bool, juce::String)> onLoaded;         // channel, ok, name or why not
    bool scanning = false; juce::String scanningName; float progress = 0;

    PartHost()
    {
        fm.addDefaultFormats();
        if (auto xml = juce::XmlDocument::parse (listFile())) known.recreateFromXml (*xml);
    }
    ~PartHost() override { stopTimer(); const juce::SpinLock::ScopedLockType sl (lock); for (auto& t : fxWin) for (auto& w : t) w.reset(); for (auto& t : fx) for (auto& p : t) p.reset(); for (auto& w : windows) w.reset(); for (auto& i : inst) i.reset(); }

    void prepare (double rate, int blockSize)
    {
        sr = rate; bs = juce::jmax (32, blockSize);
        const juce::SpinLock::ScopedLockType sl (lock);
        for (auto& i : inst) if (i) { i->releaseResources(); i->setRateAndBufferSizeDetails (sr, bs); i->prepareToPlay (sr, bs); }
        for (auto& t : fx) for (auto& p : t) if (p) { p->releaseResources(); p->setRateAndBufferSizeDetails (sr, bs); p->prepareToPlay (sr, bs); }
        temp.setSize (maxChannels, bs * 2, false, false, true);
        fxTemp.setSize (maxChannels, bs * 2, false, false, true);
    }

    void startScan()
    {
        if (scanning) return;
        scanning = true; fmtIndex = 0; progress = 0; nextFormat(); startTimer (5);
    }

    juce::var listVar() const
    {
        juce::Array<juce::var> a;
        juce::StringArray seen;
        for (const auto& d : known.getTypes())
        {
            // not instruments to run in here: Semitone itself, a DAW, and a helper that crashes when opened this way
            if (! d.isInstrument || d.name == "Semitone Studio" || d.name == "FL Studio" || d.name == "Splice Bridge") continue;
            if (seen.contains (d.name)) continue;      // the same instrument as AU and VST3: show it once
            seen.add (d.name);
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", d.createIdentifierString()); o->setProperty ("name", d.name); o->setProperty ("maker", d.manufacturerName); o->setProperty ("format", d.pluginFormatName);
            a.add (juce::var (o));
        }
        return a;
    }

    void load (int ch, const juce::String& id, const juce::String& stateB64)
    {
        if (ch < 0 || ch > 15) return;
        clear (ch);
        if (id.isEmpty()) return;
        auto d = known.getTypeForIdentifierString (id);
        if (d == nullptr) { if (onLoaded) onLoaded (ch, false, "That plug-in isn't on this computer"); return; }
        fm.createPluginInstanceAsync (*d, sr, bs, [this, ch, stateB64] (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& err)
        {
            if (p == nullptr) { if (onLoaded) onLoaded (ch, false, err); return; }
            // leave its buses alone: some instruments crash if outputs are switched off. process() gives room for them all and uses the first two.
            p->setRateAndBufferSizeDetails (sr, bs);
            if (stateB64.isNotEmpty()) { juce::MemoryBlock mb; if (mb.fromBase64Encoding (stateB64)) p->setStateInformation (mb.getData(), (int) mb.getSize()); }
            p->prepareToPlay (sr, bs);
            const auto name = p->getName();
            { const juce::SpinLock::ScopedLockType sl (lock); inst[(size_t) ch] = std::move (p); }
            if (onLoaded) onLoaded (ch, true, name);
        });
    }

    void clear (int ch)
    {
        windows[(size_t) ch].reset();
        std::unique_ptr<juce::AudioPluginInstance> old;
        { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (inst[(size_t) ch]); ccLast[(size_t) ch].fill (-1); }
    }

    void openWindow (int ch)
    {
        auto* p = inst[(size_t) ch].get();
        if (p == nullptr || ! p->hasEditor() || windows[(size_t) ch]) return;
        windows[(size_t) ch] = std::make_unique<HostWindow> (*p, [this, ch] { juce::MessageManager::callAsync ([this, ch] { windows[(size_t) ch].reset(); }); });
    }

    juce::String stateB64 (int ch)
    {
        auto* p = inst[(size_t) ch].get();
        if (p == nullptr) return {};
        juce::MemoryBlock mb; p->getStateInformation (mb);
        return mb.toBase64Encoding();
    }

    void testNote (int ch) { testCh = ch; }

    // Your effects. Each part (by channel) and the whole song (target 16) has up to four, in order.
    static constexpr int masterFx = 16, fxSlots = 4;

    juce::var effectsVar() const
    {
        juce::Array<juce::var> a;
        juce::StringArray seen;
        for (const auto& d : known.getTypes())
        {
            const auto n = d.name;
            if (d.isInstrument || n == "Semitone Studio" || seen.contains (n)) continue;
            // Apple's plumbing, and the mono versions of plug-ins that also come in stereo
            if (d.manufacturerName == "Apple" && ! juce::StringArray { "AUBandpass", "AUDelay", "AUDistortion", "AUDynamicsProcessor", "AUFilter", "AUGraphicEQ", "AUHighShelfFilter", "AUHipass", "AULowShelfFilter", "AULowpass", "AUMatrixReverb", "AUMultibandCompressor", "AUNBandEQ", "AUParametricEQ", "AUPeakLimiter", "AUPitch", "AUReverb2", "AUSampleDelay" }.contains (n)) continue;
            if (n.endsWith (" MIDIFX") || n.endsWith (" MFX") || n.endsWith (" Control") || n.endsWith (" Listener") || n.endsWith ("(m)") || n.endsWith ("(m->s)") || n.endsWithIgnoreCase (" Mono") || n.containsIgnoreCase ("Mono/Stereo")) continue;
            seen.add (n);
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", d.createIdentifierString()); o->setProperty ("name", n); o->setProperty ("maker", d.manufacturerName);
            a.add (juce::var (o));
        }
        return a;
    }

    std::function<void (int, int, bool, juce::String)> onFxLoaded;   // target, slot, ok, name or why not

    void fxLoad (int t, int slot, const juce::String& id, const juce::String& stateB64)
    {
        if (t < 0 || t > masterFx || slot < 0 || slot >= fxSlots) return;
        fxWin[(size_t) t][(size_t) slot].reset();
        std::unique_ptr<juce::AudioPluginInstance> old;
        { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (fx[(size_t) t][(size_t) slot]); }
        old.reset();
        if (id.isEmpty()) return;
        auto d = known.getTypeForIdentifierString (id);
        if (d == nullptr) { if (onFxLoaded) onFxLoaded (t, slot, false, "That effect isn't on this computer"); return; }
        fm.createPluginInstanceAsync (*d, sr, bs, [this, t, slot, stateB64] (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& err)
        {
            if (p == nullptr) { if (onFxLoaded) onFxLoaded (t, slot, false, err); return; }
            p->setRateAndBufferSizeDetails (sr, bs);
            if (stateB64.isNotEmpty()) { juce::MemoryBlock mb; if (mb.fromBase64Encoding (stateB64)) p->setStateInformation (mb.getData(), (int) mb.getSize()); }
            p->prepareToPlay (sr, bs);
            const auto name = p->getName();
            { const juce::SpinLock::ScopedLockType sl (lock); fx[(size_t) t][(size_t) slot] = std::move (p); }
            if (onFxLoaded) onFxLoaded (t, slot, true, name);
        });
    }

    void fxOpen (int t, int slot)
    {
        if (t < 0 || t > masterFx || slot < 0 || slot >= fxSlots) return;
        auto* p = fx[(size_t) t][(size_t) slot].get();
        auto& w = fxWin[(size_t) t][(size_t) slot];
        if (p == nullptr || ! p->hasEditor() || w) return;
        w = std::make_unique<HostWindow> (*p, [this, t, slot] { juce::MessageManager::callAsync ([this, t, slot] { fxWin[(size_t) t][(size_t) slot].reset(); }); });
    }

    juce::String fxState (int t, int slot) const
    {
        auto* p = (t >= 0 && t <= masterFx && slot >= 0 && slot < fxSlots) ? fx[(size_t) t][(size_t) slot].get() : nullptr;
        if (p == nullptr) return {};
        juce::MemoryBlock mb; p->getStateInformation (mb);
        return mb.toBase64Encoding();
    }

    // Audio thread: the whole-song effects, after everything is mixed.
    void processMaster (float* L, float* R, int n)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (sl.isLocked() && n <= fxTemp.getNumSamples()) runFx (masterFx, L, R, n);
    }

    // Presets the plug-in lists itself. Many list nothing useful ("Prog 1", "<disabled>"), so those are left out.
    juce::var programsVar (int ch) const
    {
        juce::Array<juce::var> a;
        auto* p = (ch >= 0 && ch < 16) ? inst[(size_t) ch].get() : nullptr;
        if (p == nullptr || p->getNumPrograms() < 2) return a;
        juce::StringArray seen;
        for (int i = 0; i < juce::jmin (p->getNumPrograms(), 1000); ++i)
        {
            const auto n = p->getProgramName (i).trim();
            if (n.isEmpty() || n.startsWith ("<") || n.matchesWildcard ("Prog *", true) || n.equalsIgnoreCase ("Init") || seen.contains (n)) continue;
            seen.add (n);
            auto* o = new juce::DynamicObject();
            o->setProperty ("i", i); o->setProperty ("name", n);
            a.add (juce::var (o));
        }
        return a;
    }

    void setProgram (int ch, int i)
    {
        auto* p = (ch >= 0 && ch < 16) ? inst[(size_t) ch].get() : nullptr;
        if (p == nullptr || i < 0 || i >= p->getNumPrograms()) return;
        const juce::SpinLock::ScopedLockType sl (lock);
        p->setCurrentProgram (i);
    }

    void setState (int ch, const juce::String& b64)
    {
        auto* p = (ch >= 0 && ch < 16) ? inst[(size_t) ch].get() : nullptr;
        juce::MemoryBlock mb;
        if (p == nullptr || b64.isEmpty() || ! mb.fromBase64Encoding (b64)) return;
        const juce::SpinLock::ScopedLockType sl (lock);
        p->setStateInformation (mb.getData(), (int) mb.getSize());
    }

    void setControl (int ch, int i, float v)
    {
        auto* p = (ch >= 0 && ch < 16) ? inst[(size_t) ch].get() : nullptr;
        if (p != nullptr && i >= 0 && i < p->getParameters().size()) p->getParameters()[i]->setValue (juce::jlimit (0.0f, 1.0f, v));
    }

    // Your saved plug-in sounds: one folder per plug-in, kept on this computer, usable in any song.
    static juce::File soundFolder (const juce::String& plugin) { return st::files::appData().getChildFile ("Plug-in sounds").getChildFile (juce::File::createLegalFileName (plugin)); }

    juce::var soundsVar (const juce::String& plugin) const
    {
        juce::Array<juce::var> a;
        for (const auto& f : soundFolder (plugin).findChildFiles (juce::File::findFiles, false, "*.json"))
            if (auto v = juce::JSON::parse (f); v.isObject()) a.add (v);
        return a;
    }

    bool saveSound (int ch, const juce::String& plugin, const juce::String& name, const juce::var& tags)
    {
        const auto state = stateB64 (ch);
        if (state.isEmpty()) return false;
        auto dir = soundFolder (plugin); dir.createDirectory();
        const auto base = juce::File::createLegalFileName (name);
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", name); o->setProperty ("tags", tags);
        return st::files::writeSafely (dir.getChildFile (base + ".state"), state) && st::files::writeSafely (dir.getChildFile (base + ".json"), juce::JSON::toString (juce::var (o)));
    }

    void loadSound (int ch, const juce::String& plugin, const juce::String& name)
    {
        setState (ch, soundFolder (plugin).getChildFile (juce::File::createLegalFileName (name) + ".state").loadFileAsString());
    }

    // Removed sounds go to a _to_delete folder, never straight to nothing.
    void removeSound (const juce::String& plugin, const juce::String& name)
    {
        auto dir = soundFolder (plugin), bin = dir.getChildFile ("_to_delete"); bin.createDirectory();
        const auto base = juce::File::createLegalFileName (name);
        for (const auto* ext : { ".json", ".state" })
        {
            auto f = dir.getChildFile (base + ext);
            if (f.existsAsFile()) f.moveFileTo (bin.getNonexistentChildFile (base, ext));
        }
    }

    // The controls worth drawing: automatable, named, and not the long lists of MIDI CC stand-ins plug-ins add.
    juce::var paramsVar (int ch, const juce::var& also = {}) const
    {
        juce::Array<juce::var> first, rest;
        auto* p = inst[(size_t) ch].get();
        if (p == nullptr) return first;
        static const char* useful[] { "macro", "cutoff", "reson", "volume", "drive", "reverb", "delay", "chorus", "attack", "release", "filter", "mix", "pan", "easy edit", "swell", "crescendo", "pedal", "gain", "output", "distortion", "wt pos" };
        const auto& ps = p->getParameters();
        for (int i = 0; i < ps.size(); ++i)
        {
            auto* q = ps[i];
            const auto n = q->getName (64);
            if (! q->isAutomatable() || n.isEmpty() || n.startsWithIgnoreCase ("MIDI CC") || n.startsWithIgnoreCase ("CC") || n.startsWithChar ('#') || n.containsOnly ("0123456789")) continue;
            auto* o = new juce::DynamicObject();
            o->setProperty ("i", i); o->setProperty ("name", n); o->setProperty ("v", q->getValue());
            bool top = false;
            for (auto* k : useful) top = top || n.containsIgnoreCase (k);
            (top ? first : rest).add (juce::var (o));
        }
        // the controls people reach for first, then the rest, up to what a menu can hold
        for (const auto& v : rest) { if (first.size() >= 400) break; first.add (v); }
        first.removeLast (juce::jmax (0, first.size() - 400));
        // controls you taught Semitone, which the list above may leave out
        if (auto* extra = also.getArray())
            for (const auto& x : *extra)
            {
                const int i = (int) x;
                if (i < 0 || i >= ps.size()) continue;
                bool have = false;
                for (const auto& v : first) have = have || (int) v.getProperty ("i", -1) == i;
                if (have) continue;
                auto* o = new juce::DynamicObject();
                o->setProperty ("i", i); o->setProperty ("name", ps[i]->getName (64)); o->setProperty ("v", ps[i]->getValue());
                first.add (juce::var (o));
            }
        return first;
    }

    // Learn: watch every control, including unnamed ones, for the one you move in the plug-in's window.
    // The first half second only marks controls that move by themselves, so they can't be mistaken for your knob.
    void learnStart (int ch)
    {
        learnCh = -1;
        auto* p = (ch >= 0 && ch < 16) ? inst[(size_t) ch].get() : nullptr;
        if (p == nullptr) return;
        const auto& ps = p->getParameters();
        learnBase.resize ((size_t) ps.size()); learnNoisy.assign ((size_t) ps.size(), false);
        for (int i = 0; i < ps.size(); ++i) learnBase[(size_t) i] = ps[i]->getValue();
        learnCh = ch; learnPolls = 0; learnSettle = 0;
    }

    void learnStop() { learnCh = -1; }
    int learning() const { return learnCh; }

    // Message thread, about 20 times a second. True once a control has clearly moved.
    bool learnPoll (int& index, juce::String& name, float& value)
    {
        auto* p = (learnCh >= 0) ? inst[(size_t) learnCh].get() : nullptr;
        if (p == nullptr) return false;
        const auto& ps = p->getParameters();
        if ((size_t) ps.size() != learnBase.size()) return false;
        ++learnPolls;
        float best = 0; int at = -1;
        for (int i = 0; i < ps.size(); ++i)
        {
            const float d = std::abs (ps[i]->getValue() - learnBase[(size_t) i]);
            if (learnPolls <= 10) { if (d > .0005f) learnNoisy[(size_t) i] = true; continue; }
            if (! learnNoisy[(size_t) i] && d > best) { best = d; at = i; }
        }
        if (at < 0 || best < .015f) return false;
        if (++learnSettle < 6) return false;      // let the knob travel, so the biggest mover wins over linked controls
        index = at; name = ps[at]->getName (64); value = ps[at]->getValue();
        learnCh = -1;
        return true;
    }

    // Audio thread, inside process(): lanes are applied before each block.
    void setLanes (std::shared_ptr<const std::vector<Lane>> l) { std::atomic_store (&lanes, std::move (l)); }

    bool hasAny() const { for (const auto& i : inst) if (i) return true; return false; }

    // Audio thread: skips the block if a plug-in is being swapped.
    void process (const std::vector<Event>& events, int n, float* L, float* R, double beat = -1)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || n > temp.getNumSamples()) return;
        // While playing, lanes set the controls. On stop each goes back to its resting value (a lane's first
        // point), so stopping in the gap of a tremolo doesn't leave the plug-in silent.
        const bool stopped = beat < 0 && lanesMoving;
        if (beat >= 0 || stopped)
            if (auto list = std::atomic_load (&lanes))
                for (const auto& k : *list)
                    if (k.ch >= 0 && k.ch < 16 && inst[(size_t) k.ch] && ! k.pts.empty())
                    {
                        const auto& ps = inst[(size_t) k.ch]->getParameters();
                        const float v = juce::jlimit (0.0f, 1.0f, stopped ? k.pts.front().second : k.at (beat));
                        if (k.fxParam >= ccBase) queueCc (k.ch, k.fxParam - ccBase, juce::roundToInt (v * 127.0f));
                        else if (k.fxParam >= 0 && k.fxParam < ps.size()) ps[k.fxParam]->setValue (v);
                    }
        lanesMoving = beat >= 0;
        // Link by MIDI: a slow sweep up and down, for the plug-in's own MIDI Learn to catch
        if (wiggleLeft > 0)
        {
            const int total = (int) (sr * 1.5), done = total - wiggleLeft, half = total / 2;
            const int v = juce::jlimit (0, 127, done < half ? done * 127 / juce::jmax (1, half) : (total - done) * 127 / juce::jmax (1, half));
            queueCc (wiggleCh.load(), wiggleCc.load(), v);
            wiggleLeft -= n;
        }
        for (int ch = 0; ch < 16; ++ch)
        {
            auto* p = inst[(size_t) ch].get();
            if (p == nullptr) continue;
            midi.clear();
            for (int q = 0; q < ccCount; ++q)
                if (ccOut[(size_t) q].ch == ch) midi.addEvent (juce::MidiMessage::controllerEvent (1, ccOut[(size_t) q].cc, ccOut[(size_t) q].v), 0);
            for (const auto& e : events)
                if ((e.status & 15) == ch) midi.addEvent (juce::MidiMessage (e.status & 0xF0, e.d1, e.d2), e.offset);
            if (testCh.load() == ch) { testCh = -1; heldCh = ch; heldLeft = (int) (sr * .7); midi.addEvent (juce::MidiMessage (0x90, 60, 100), 0); }
            if (heldCh == ch && (heldLeft -= n) <= 0) { heldCh = -1; midi.addEvent (juce::MidiMessage (0x80, 60, 0), 0); }
            const int outs = juce::jmax (2, p->getTotalNumOutputChannels(), p->getTotalNumInputChannels());
            if (outs > maxChannels) continue;
            juce::AudioBuffer<float> view (temp.getArrayOfWritePointers(), outs, n);
            view.clear();
            p->processBlock (view, midi);
            runFx (ch, view.getWritePointer (0), view.getWritePointer (1), n);
            juce::FloatVectorOperations::add (L, view.getReadPointer (0), n);
            if (R != nullptr && R != L) juce::FloatVectorOperations::add (R, view.getReadPointer (1), n);
        }
        ccCount = 0;
    }

    static constexpr int ccBase = 100000;   // a lane with an index from here sends MIDI controller (index - ccBase)
    void sendCcSweep (int ch, int cc) { wiggleCh = ch; wiggleCc = juce::jlimit (0, 127, cc); wiggleLeft = (int) (sr * 1.5); }

private:
    static juce::File listFile() { return st::files::appData().getChildFile ("my-plugins.xml"); }
    static juce::File deadFile() { return st::files::appData().getChildFile ("plugins-being-checked.txt"); }

    void nextFormat()
    {
        scanner.reset();
        while (fmtIndex < fm.getNumFormats())
        {
            auto* f = fm.getFormat (fmtIndex++);
            scanner = std::make_unique<juce::PluginDirectoryScanner> (known, *f, f->getDefaultLocationsToSearch(), true, deadFile(), false);
            return;
        }
        scanning = false; stopTimer();
        if (auto xml = known.createXml()) xml->writeTo (listFile());
        if (onList) onList();
    }

    void timerCallback() override
    {
        if (! scanner) { nextFormat(); return; }
        juce::String name;
        const bool more = scanner->scanNextFile (true, name);
        scanningName = name; progress = scanner->getProgress();
        if (onList && ++ticks % 8 == 0) onList();
        if (! more) nextFormat();
    }

    juce::AudioPluginFormatManager fm;
    juce::KnownPluginList known;
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    int fmtIndex = 0, ticks = 0, bs = 512;
    double sr = 48000;
    std::array<std::unique_ptr<juce::AudioPluginInstance>, 16> inst;
    std::array<std::unique_ptr<HostWindow>, 16> windows;
    juce::SpinLock lock;
    std::atomic<int> testCh { -1 };
    std::shared_ptr<const std::vector<Lane>> lanes;
    void runFx (int t, float* L, float* R, int n)
    {
        for (auto& p : fx[(size_t) t])
        {
            if (p == nullptr) continue;
            const int chans = juce::jmax (2, p->getTotalNumOutputChannels(), p->getTotalNumInputChannels());
            if (chans > maxChannels) continue;
            juce::AudioBuffer<float> view (fxTemp.getArrayOfWritePointers(), chans, n);
            view.clear();
            view.copyFrom (0, 0, L, n);
            view.copyFrom (1, 0, R != nullptr ? R : L, n);
            juce::MidiBuffer none;
            p->processBlock (view, none);
            juce::FloatVectorOperations::copy (L, view.getReadPointer (0), n);
            if (R != nullptr && R != L) juce::FloatVectorOperations::copy (R, view.getReadPointer (p->getTotalNumOutputChannels() > 1 ? 1 : 0), n);
        }
    }

    std::array<std::array<std::unique_ptr<juce::AudioPluginInstance>, 4>, 17> fx;
    std::array<std::array<std::unique_ptr<HostWindow>, 4>, 17> fxWin;
    juce::AudioBuffer<float> fxTemp { maxChannels, 8192 };
    bool lanesMoving = false;
    struct CcOut { int ch, cc, v; };
    std::array<CcOut, 64> ccOut {};
    int ccCount = 0;
    std::array<std::array<int, 128>, 16> ccLast = [] { std::array<std::array<int, 128>, 16> a {}; for (auto& c : a) c.fill (-1); return a; }();
    std::atomic<int> wiggleCh { 0 }, wiggleCc { 0 }, wiggleLeft { 0 };
    void queueCc (int ch, int cc, int v)
    {
        if (ch < 0 || ch > 15 || cc < 0 || cc > 127 || ccLast[(size_t) ch][(size_t) cc] == v || ccCount >= (int) ccOut.size()) return;
        ccLast[(size_t) ch][(size_t) cc] = v;
        ccOut[(size_t) ccCount++] = { ch, cc, v };
    }
    int learnCh = -1, learnPolls = 0, learnSettle = 0;
    std::vector<float> learnBase;
    std::vector<bool> learnNoisy;
    int heldCh = -1, heldLeft = 0;
    static constexpr int maxChannels = 128;
    juce::AudioBuffer<float> temp { maxChannels, 8192 };
    juce::MidiBuffer midi;
};
}
