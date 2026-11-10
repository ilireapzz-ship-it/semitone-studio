#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Sequencer.h"
#include "Synth.h"
#include "SongFiles.h"
#include <thread>

// Phase 3, desktop app only: your own instrument plug-ins on Semitone's parts. Each MIDI channel can hold one.
// Finding plug-ins opens each one, so it runs a file at a time on request, and a plug-in that crashes is skipped next time.
namespace st
{
// The song's position for hosted plug-ins, so anything synced to the host follows Semitone.
struct SongHead : juce::AudioPlayHead
{
    std::atomic<double> bpm { 120 }, ppq { 0 }, loopA { 0 }, loopB { 0 };
    std::atomic<bool> playing { false }, looping { false };
    std::atomic<int> top { 4 }, bot { 4 };
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm.load()); p.setPpqPosition (ppq.load()); p.setIsPlaying (playing.load());
        p.setTimeSignature (TimeSignature { top.load(), bot.load() });
        p.setTimeInSeconds (ppq.load() * 60.0 / juce::jmax (1.0, bpm.load()));
        p.setIsLooping (looping.load());
        if (looping.load()) p.setLoopPoints (LoopPoints { loopA.load(), loopB.load() });
        return p;
    }
};

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
        keepAboveApp();
        raise();
        // the click that opened it came from the web view, which can take the focus back: raise it again a moment later
        juce::Component::SafePointer<HostWindow> safe (this);
        juce::Timer::callAfterDelay (150, [safe] { if (safe != nullptr) safe->raise(); });
    }
    void closeButtonPressed() override { if (onClosed) onClosed(); }
    // Jamie, 11 Nov: a plug-in opened behind the app, and clicking its name again did nothing. Now it comes to the front every time.
    void raise() { if (isMinimised()) setMinimised (false); setVisible (true); toFront (true); }
private:
    // Windows: make the plug-in window owned by Semitone's own window, like FL Studio's plug-in windows: it stays above the app
    // (never hidden behind it) and minimises with it, without floating over other programs. Plug-in builds inside a DAW have no
    // Semitone top-level window, so this does nothing there.
    void keepAboveApp()
    {
       #if JUCE_WINDOWS
        auto* peer = getPeer();
        if (peer == nullptr) return;
        for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i)
        {
            auto* w = juce::TopLevelWindow::getTopLevelWindow (i);
            if (w == nullptr || w == this || dynamic_cast<HostWindow*> (w) != nullptr || ! w->isShowing()) continue;
            if (auto* mp = w->getPeer())
            {
                static juce::DynamicLibrary user32 ("user32.dll");
                using SetLong = intptr_t (__stdcall*) (void*, int, intptr_t);
                if (auto fn = (SetLong) user32.getFunction ("SetWindowLongPtrW"))
                    fn (peer->getNativeHandle(), -8 /* GWLP_HWNDPARENT: the owner window */, (intptr_t) mp->getNativeHandle());
            }
            break;
        }
       #endif
    }
    std::function<void()> onClosed;
};

class PartHost : private juce::Timer
{
public:
    std::function<void()> onList;                                   // the plug-in list changed
    std::function<void (bool, juce::String)> onBridge;              // loaded, name or why not
    SongHead head;
    std::function<void (int, bool, juce::String)> onLoaded;         // channel, ok, name or why not
    bool scanning = false; juce::String scanningName; float progress = 0;

    PartHost()
    {
        fm.addDefaultFormats();
        if (auto xml = juce::XmlDocument::parse (listFile())) known.recreateFromXml (*xml);
    }
    ~PartHost() override { aliveFlag->store (false); stopTimer(); scanThread.reset(); const juce::SpinLock::ScopedLockType sl (lock); for (auto& t : fxWin) for (auto& w : t) w.reset(); for (auto& t : fx) for (auto& p : t) p.reset(); for (auto& w : windows) w.reset(); for (auto& i : inst) i.reset(); }

    void prepare (double rate, int blockSize)
    {
        sr = rate; bs = juce::jmax (32, blockSize);
        const juce::SpinLock::ScopedLockType sl (lock);
        for (auto& i : inst) if (i) { i->releaseResources(); i->setRateAndBufferSizeDetails (sr, bs); i->prepareToPlay (sr, bs); }
        for (auto& t : fx) for (auto& p : t) if (p) { p->releaseResources(); p->setRateAndBufferSizeDetails (sr, bs); p->prepareToPlay (sr, bs); }
        temp.setSize (maxChannels, bs * 2, false, false, true);
        fxTemp.setSize (maxChannels, bs * 2, false, false, true);
        partOut.setSize (32, bs * 2, false, false, true);
    }

    void startScan()
    {
        if (scanning) return;
        scanning = true; progress = 0; scanShare = 0; scanningName = {};
        // Each plug-in file is opened by a short-lived copy of this app, so one that crashes takes only that copy down.
        scanThread.reset (new ScanThread (*this));
        scanThread->startThread();
        startTimer (400);
    }

    juce::var listVar() const
    {
        juce::Array<juce::var> a;
        juce::StringArray seen;
        for (const auto& d : known.getTypes())
        {
            // not instruments for a part: Semitone itself, a DAW, and Splice Bridge (an audition tool, see bridgeLoad)
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
        const auto gen = ++loadGen[(size_t) ch];   // a later pick (or removing it) wins over a load still on its way
        if (id.isEmpty()) return;
        auto d = known.getTypeForIdentifierString (id);
        if (d == nullptr) { if (onLoaded) onLoaded (ch, false, "That plug-in isn't on this computer"); return; }
        fm.createPluginInstanceAsync (*d, sr, bs, [this, ch, stateB64, gen, alive = aliveFlag] (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& err)
        {
            if (! alive->load() || gen != loadGen[(size_t) ch]) return;
            if (p == nullptr) { if (onLoaded) onLoaded (ch, false, err); return; }
            // leave its buses alone: some instruments crash if outputs are switched off. process() gives room for them all and uses the first two.
            p->setRateAndBufferSizeDetails (sr, bs);
            p->setPlayHead (&head);
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

    // Splice Bridge is an audition tool, not a part: it has no strip, isn't exported, and goes straight to the output.
    bool hasBridge() const { for (const auto& d : known.getTypes()) if (d.name == "Splice Bridge") return true; return false; }
    void bridgeLoad()
    {
        if (audition != nullptr) { if (onBridge) onBridge (true, audition->getName()); return; }
        std::unique_ptr<juce::PluginDescription> d;
        for (const auto& t : known.getTypes()) if (t.name == "Splice Bridge") { d = std::make_unique<juce::PluginDescription> (t); break; }
        if (d == nullptr) { if (onBridge) onBridge (false, "Splice Bridge isn't on this computer"); return; }
        fm.createPluginInstanceAsync (*d, sr, bs, [this] (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& err)
        {
            if (p == nullptr) { if (onBridge) onBridge (false, err); return; }
            p->setRateAndBufferSizeDetails (sr, bs);
            p->setPlayHead (&head);
            p->prepareToPlay (sr, bs);
            const auto name = p->getName();
            { const juce::SpinLock::ScopedLockType sl (lock); audition = std::move (p); }
            if (onBridge) onBridge (true, name);
            bridgeOpen();
        });
    }
    void bridgeOpen()
    {
        if (bridgeWin) { bridgeWin->raise(); return; }
        if (audition == nullptr || ! audition->hasEditor()) return;
        bridgeWin = std::make_unique<HostWindow> (*audition, [this] { juce::MessageManager::callAsync ([this] { bridgeWin.reset(); }); });
    }
    void bridgeClear()
    {
        bridgeWin.reset();
        std::unique_ptr<juce::AudioPluginInstance> old;
        { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (audition); }
        if (onBridge) onBridge (false, {});
    }
    void setLoop (double a, double b) { head.loopA = a; head.loopB = b; head.looping = b > a; }
    void setTransport (double bpm, double ppq, bool playing) { head.bpm = bpm; head.ppq = ppq; head.playing = playing; }

    // Audio thread, last of all: whatever Bridge is playing, straight to the output.
    void processAudition (float* L, float* R, int n)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || audition == nullptr || n > temp.getNumSamples()) return;
        const int chans = juce::jmax (2, audition->getTotalNumOutputChannels(), audition->getTotalNumInputChannels());
        if (chans > maxChannels) return;
        temp.clear();
        juce::AudioBuffer<float> view (temp.getArrayOfWritePointers(), chans, n);
        juce::MidiBuffer none;
        audition->processBlock (view, none);
        for (int i = 0; i < n; ++i) { L[i] += view.getSample (0, i); if (R != nullptr && R != L) R[i] += view.getSample (1, i); }
    }

    void openWindow (int ch)
    {
        if (ch < 0 || ch > 15) return;
        auto* p = inst[(size_t) ch].get();
        if (windows[(size_t) ch]) { windows[(size_t) ch]->raise(); return; }   // already open: bring it to the front
        if (p == nullptr || ! p->hasEditor()) return;
        windows[(size_t) ch] = std::make_unique<HostWindow> (*p, [this, ch] { juce::MessageManager::callAsync ([this, ch] { windows[(size_t) ch].reset(); }); });
    }

    juce::String stateB64 (int ch)
    {
        if (ch < 0 || ch > 15) return {};
        auto* p = inst[(size_t) ch].get();
        if (p == nullptr) return {};
        juce::MemoryBlock mb; p->getStateInformation (mb);
        return mb.toBase64Encoding();
    }

    void testNote (int ch) { testCh = ch; }

    // One note, right now, so a note placed on the piano roll is heard in the plug-in's own sound.
    void previewNote (int ch, int note, int vel, int ms)
    {
        const int v = ((juce::jlimit (0, 30, ch) + 1) << 22) | (juce::jlimit (0, 127, note) << 15) | (juce::jlimit (1, 127, vel) << 8) | juce::jlimit (1, 255, ms / 10);
        for (auto& s : previewQueue) { int empty = 0; if (s.compare_exchange_strong (empty, v)) break; }
    }

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
        const auto gen = ++fxGen[(size_t) t][(size_t) slot];
        if (id.isEmpty()) return;
        auto d = known.getTypeForIdentifierString (id);
        if (d == nullptr) { if (onFxLoaded) onFxLoaded (t, slot, false, "That effect isn't on this computer"); return; }
        fm.createPluginInstanceAsync (*d, sr, bs, [this, t, slot, stateB64, gen, alive = aliveFlag] (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& err)
        {
            if (! alive->load() || gen != fxGen[(size_t) t][(size_t) slot]) return;
            if (p == nullptr) { if (onFxLoaded) onFxLoaded (t, slot, false, err); return; }
            p->setRateAndBufferSizeDetails (sr, bs);
            if (stateB64.isNotEmpty()) { juce::MemoryBlock mb; if (mb.fromBase64Encoding (stateB64)) p->setStateInformation (mb.getData(), (int) mb.getSize()); }
            p->setPlayHead (&head);
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
        if (w) { w->raise(); return; }
        if (p == nullptr || ! p->hasEditor()) return;
        w = std::make_unique<HostWindow> (*p, [this, t, slot] { juce::MessageManager::callAsync ([this, t, slot] { fxWin[(size_t) t][(size_t) slot].reset(); }); });
    }

    juce::String fxState (int t, int slot) const
    {
        auto* p = (t >= 0 && t <= masterFx && slot >= 0 && slot < fxSlots) ? fx[(size_t) t][(size_t) slot].get() : nullptr;
        if (p == nullptr) return {};
        juce::MemoryBlock mb; p->getStateInformation (mb);
        return mb.toBase64Encoding();
    }

    // Audio thread: which of Semitone's own parts (no plug-in instrument on the channel) have your effects, one bit each.
    uint32_t builtInFxMask()
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked()) return lastBuiltInMask;
        uint32_t m = 0;
        for (int c = 0; c < 16; ++c)
        {
            if (inst[(size_t) c] != nullptr) continue;   // a plug-in instrument's effects already run in process()
            for (const auto& p : fx[(size_t) c]) if (p != nullptr) { m |= 1u << c; break; }
        }
        return lastBuiltInMask = m;
    }

    // Audio thread: run one built-in part's effects on a block of its sound.
    void processChannelFx (int t, float* L, float* R, int n)
    {
        if (t < 0 || t >= masterFx) return;
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (sl.isLocked() && n <= fxTemp.getNumSamples()) runFx (t, L, R, n);
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
        if (ch < 0 || ch > 15) return juce::var (juce::Array<juce::var>());
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
    void process (const std::vector<Event>& events, int n, float* L, float* R, double beat = -1, bool toEngine = false)
    {
        outReady.fill (false);   // a skipped block hands back silence, never last block's audio again
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || n > temp.getNumSamples() || n > partOut.getNumSamples()) return;
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

        // Timing: some plug-ins and effects hand their sound back late. Everything else waits for the slowest,
        // so each part lands together, however much delay any one of them adds.
        std::array<int, 16> late {};
        int slowest = 0;
        for (int ch = 0; ch < 16; ++ch)
            if (auto* p = inst[(size_t) ch].get())
            {
                late[(size_t) ch] = chainLatency (ch) + p->getLatencySamples();
                slowest = juce::jmax (slowest, late[(size_t) ch]);
            }
        slowest = juce::jmin (slowest, LineUp::size - 8192);
        partLatency = slowest + chainLatency (masterFx);
        slowestNow = slowest;
        if (! toEngine) lineUp[16].run (L, R, n, slowest);    // what's already in the buffer: the built-in sounds
        outReady.fill (false);
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
            for (auto& s : previewQueue)
            {
                const int v = s.load();
                if (v == 0 || (v >> 22) - 1 != ch) continue;
                s.store (0);
                const int note = (v >> 15) & 127;
                for (auto& h : previewHeld)
                    if (h.ch < 0) { h = { ch, note, (int) (sr * (v & 255) * .01) }; midi.addEvent (juce::MidiMessage (0x90, note, (juce::uint8) ((v >> 8) & 127)), 0); break; }
            }
            for (auto& h : previewHeld)
                if (h.ch == ch && (h.left -= n) <= 0) { h.ch = -1; midi.addEvent (juce::MidiMessage (0x80, h.note, (juce::uint8) 0), 0); }
            const int outs = juce::jmax (2, p->getTotalNumOutputChannels(), p->getTotalNumInputChannels());
            if (outs > maxChannels) continue;
            juce::AudioBuffer<float> view (temp.getArrayOfWritePointers(), outs, n);
            view.clear();
            p->processBlock (view, midi);
            runFx (ch, view.getWritePointer (0), view.getWritePointer (1), n);
            lineUp[(size_t) ch].run (view.getWritePointer (0), view.getWritePointer (1), n, juce::jmax (0, slowest - late[(size_t) ch]));
            if (toEngine)
            {
                juce::FloatVectorOperations::copy (partOut.getWritePointer (ch * 2), view.getReadPointer (0), n);
                juce::FloatVectorOperations::copy (partOut.getWritePointer (ch * 2 + 1), view.getReadPointer (1), n);
                outReady[(size_t) ch] = true;
                continue;
            }
            juce::FloatVectorOperations::add (L, view.getReadPointer (0), n);
            if (R != nullptr && R != L) juce::FloatVectorOperations::add (R, view.getReadPointer (1), n);
        }
        ccCount = 0;
    }

    int latencySamples() const { return partLatency.load(); }
    int slowestPart() const { return slowestNow; }           // audio thread, after process()
    const float* partLeft (int ch) const { return outReady[(size_t) ch] ? partOut.getReadPointer (ch * 2) : nullptr; }
    const float* partRight (int ch) const { return outReady[(size_t) ch] ? partOut.getReadPointer (ch * 2 + 1) : nullptr; }

    // Exporting: faster than real time, so plug-ins use their best quality and load everything they need.
    void setOffline (bool on)
    {
        for (auto& p : inst) if (p) p->setNonRealtime (on);
        for (auto& t : fx) for (auto& p : t) if (p) p->setNonRealtime (on);
    }

    static constexpr int ccBase = 100000;   // a lane with an index from here sends MIDI controller (index - ccBase)
    void sendCcSweep (int ch, int cc) { wiggleCh = ch; wiggleCc = juce::jlimit (0, 127, cc); wiggleLeft = (int) (sr * 1.5); }

private:
    static juce::File listFile() { return st::files::appData().getChildFile ("my-plugins.xml"); }
    static juce::File deadFile() { return st::files::appData().getChildFile ("plugins-being-checked.txt"); }

    void timerCallback() override { progress = scanShare.load(); scanningName = "your plug-ins"; if (onList) onList(); }

    // Runs in the short-lived copy: `app --scan-one <format> <file> <out.xml>`.
public:
    static int scanOne (const juce::StringArray& args)
    {
        const int at = args.indexOf ("--scan-one");
        if (at < 0 || args.size() < at + 4) return -1;
        juce::AudioPluginFormatManager fm;
        fm.addDefaultFormats();
        juce::OwnedArray<juce::PluginDescription> found;
        for (auto* f : fm.getFormats())
            if (f->getName() == args[at + 1]) f->findAllTypesForFile (found, args[at + 2]);
        juce::XmlElement root ("TYPES");
        for (auto* d : found) root.addChildElement (d->createXml().release());
        return root.writeTo (juce::File (args[at + 3])) ? 0 : 1;
    }
private:
    struct ScanThread : juce::Thread
    {
        explicit ScanThread (PartHost& h) : juce::Thread ("Semitone plug-in scan"), host (h) {}
        ~ScanThread() override { stopThread (70000); }
        void run() override
        {
            struct Job { juce::String format, file; };
            std::vector<Job> jobs;
            for (auto* f : host.fm.getFormats())
                for (const auto& file : f->searchPathsForPlugins (f->getDefaultLocationsToSearch(), true, false))
                    if (! host.known.isListingUpToDate (file, *f)) jobs.push_back ({ f->getName(), file });
            const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
            auto dir = st::files::appData().getChildFile ("scan-temp");
            dir.createDirectory();
            std::atomic<size_t> next { 0 }; std::atomic<int> done { 0 };
            auto worker = [&] (int id)
            {
                for (size_t i = next++; i < jobs.size() && ! threadShouldExit(); i = next++)
                {
                    const auto out = dir.getChildFile ("s" + juce::String (id) + ".xml");
                    out.deleteFile();
                    juce::ChildProcess p;
                    if (p.start (juce::StringArray { exe, "--scan-one", jobs[i].format, jobs[i].file, out.getFullPathName() }, 0))
                    {
                        if (! p.waitForProcessToFinish (60000)) p.kill();
                        else if (p.getExitCode() == 0)
                            if (auto x = juce::parseXML (out))
                                for (auto* e : x->getChildIterator())
                                {
                                    juce::PluginDescription d;
                                    if (d.loadFromXml (*e)) host.known.addType (d);
                                }
                    }
                    host.scanShare = (float) (++done) / (float) juce::jmax ((size_t) 1, jobs.size());
                }
            };
            {
                std::vector<std::thread> crew;
                for (int i = 0; i < 4; ++i) crew.emplace_back (worker, i);
                for (auto& t : crew) t.join();
            }
            dir.deleteRecursively();
            if (auto xml = host.known.createXml()) xml->writeTo (host.listFile());
            juce::MessageManager::callAsync ([h = &host, alive = host.aliveFlag]
            {
                if (! alive->load()) return;
                h->stopTimer(); h->scanning = false;
                if (h->onList) h->onList();
            });
        }
        PartHost& host;
    };
    std::unique_ptr<ScanThread> scanThread;
    std::atomic<float> scanShare { 0 };
    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>> (true);
    std::array<juce::uint32, 16> loadGen {};                       // message thread: which load of each part is the latest
    std::array<std::array<juce::uint32, fxSlots>, masterFx + 1> fxGen {};

    juce::AudioPluginFormatManager fm;
    juce::KnownPluginList known;
    int bs = 512;
    double sr = 48000;
    std::unique_ptr<juce::AudioPluginInstance> audition;
    std::unique_ptr<HostWindow> bridgeWin;
    std::array<std::unique_ptr<juce::AudioPluginInstance>, 16> inst;
    std::array<std::unique_ptr<HostWindow>, 16> windows;
    juce::SpinLock lock;
    std::atomic<int> testCh { -1 };
    std::array<std::atomic<int>, 12> previewQueue {};
    struct PreviewHeld { int ch = -1, note = 0, left = 0; };
    std::array<PreviewHeld, 12> previewHeld {};
    std::shared_ptr<const std::vector<Lane>> lanes;
    struct LineUp
    {
        static constexpr int size = 1 << 16;      // about 1.4 s at 48 kHz
        std::vector<float> l = std::vector<float> ((size_t) size, 0.0f), r = std::vector<float> ((size_t) size, 0.0f);
        int at = 0;
        void run (float* L, float* R, int n, int delay)
        {
            if (delay <= 0) return;
            for (int i = 0; i < n; ++i)
            {
                const int back = (at - delay) & (size - 1);
                l[(size_t) at] = L[i]; L[i] = l[(size_t) back];
                if (R != nullptr && R != L) { r[(size_t) at] = R[i]; R[i] = r[(size_t) back]; }
                at = (at + 1) & (size - 1);
            }
        }
    };
    std::array<LineUp, 17> lineUp;
    std::atomic<int> partLatency { 0 };
    int chainLatency (int t) const { int sum = 0; for (const auto& p : fx[(size_t) t]) if (p) sum += p->getLatencySamples(); return sum; }

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
    uint32_t lastBuiltInMask = 0;
    juce::AudioBuffer<float> partOut { 32, 8192 };
    std::array<bool, 16> outReady {};
    int slowestNow = 0;
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
