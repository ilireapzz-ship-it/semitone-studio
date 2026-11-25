#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Sequencer.h"
#include "Synth.h"
#include "SongFiles.h"
#include <bitset>
#include <cstdint>
#include <cstring>
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
    std::atomic<int64_t> samples { 0 };       // 25 Nov: plug-ins were always told sample 0 and 4/4
    std::atomic<double> rate { 48000 };
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm.load()); p.setPpqPosition (ppq.load()); p.setIsPlaying (playing.load());
        p.setTimeSignature (TimeSignature { top.load(), bot.load() });
        p.setTimeInSamples (samples.load());
        p.setTimeInSeconds ((double) samples.load() / juce::jmax (1.0, rate.load()));
        const double bar = juce::jmax (.25, top.load() * 4.0 / juce::jmax (1, bot.load()));
        p.setPpqPositionOfLastBarStart (std::floor (ppq.load() / bar) * bar);
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
    // 22 Nov (Jamie: "when i click x on a plugin, it minimizes the daw"): Windows gives the focus to whatever window is next when the
    // active one closes, often another program, so Semitone looked minimised. Semitone's own window comes to the front first.
    void closeButtonPressed() override
    {
        if (owner != nullptr) { if (auto* op = owner->getPeer()) if (op->isMinimised()) op->setMinimised (false); owner->toFront (true); }
        if (onClosed) onClosed();
    }
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
            owner = w;
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
    juce::Component::SafePointer<juce::TopLevelWindow> owner;   // Semitone's own window (Windows app only)
};

class PartHost : private juce::Timer
{
public:
    std::function<void()> onList;                                   // the plug-in list changed
    std::function<void (bool, juce::String)> onBridge;              // loaded, name or why not
    SongHead head;
    std::function<void (int, bool, juce::String)> onLoaded;         // channel, ok, name or why not
    std::function<void (const juce::String&)> onLog;               // a line for the start-up log

    // 25 Nov: some plug-ins (ValhallaVintageVerb) leave their main input and output off until the host switches them on, so an effect
    // came up with 0 in and 0 out and gave back silence. Only the main pair is touched: sidechains and extra outputs stay as they are.
    // The plug-in must not be running. Called before and after its saved sound goes in.
    static void openMain (juce::AudioPluginInstance& p, bool effect)
    {
        const bool hasIn = p.getBusCount (true) > 0, hasOut = p.getBusCount (false) > 0;
        const int ins = p.getMainBusNumInputChannels(), outs = p.getMainBusNumOutputChannels();
        const bool inOff = effect && hasIn && ins == 0, outOff = hasOut && outs == 0;
        const bool narrow = effect && hasIn && hasOut && ! inOff && ! outOff && (ins == 1 || outs == 1);   // a mono effect: in stereo if it can
        if (! inOff && ! outOff && ! narrow) return;
        p.releaseResources();
        const auto now = p.getBusesLayout();
        auto put = [&] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
        {
            auto want = now;
            if (effect && hasIn) { if (in.isDisabled()) return false; want.inputBuses.getReference (0) = in; }
            if (hasOut) { if (out.isDisabled()) return false; want.outputBuses.getReference (0) = out; }
            return p.setBusesLayout (want);     // asks the plug-in first, and leaves everything as it was if it says no
        };
        const auto stereo = juce::AudioChannelSet::stereo(), mono = juce::AudioChannelSet::mono();
        if (put (stereo, stereo) || narrow) return;
        const auto lastIn = hasIn ? p.getBus (true, 0)->getLastEnabledLayout() : juce::AudioChannelSet::disabled();
        const auto lastOut = hasOut ? p.getBus (false, 0)->getLastEnabledLayout() : juce::AudioChannelSet::disabled();
        if (put (lastIn, lastOut) || put (mono, stereo)) return;
        put (mono, mono);
    }

    // 21 Nov (Jamie: "how come no sound is coming from opus?"; it works in FL). process() skips a plug-in with more channels than its buffer,
    // so a plug-in with many multi-outs never ran at all, not even from its own keyboard. Its buses are left alone (some instruments crash
    // when outputs are switched off) unless there are more than the buffer holds; then only its main ones stay on (we use the first pair).
    void fitChannels (juce::AudioPluginInstance& p)
    {
        auto count = [&p] { return juce::jmax (p.getTotalNumOutputChannels(), p.getTotalNumInputChannels()); };
        juce::String say = p.getName() + ": " + juce::String (p.getTotalNumInputChannels()) + " in, " + juce::String (p.getTotalNumOutputChannels())
                         + " out, " + juce::String (p.getBusCount (false)) + " output buses";
        if (count() > maxChannels)
        {
            p.disableNonMainBuses();
            say << "; more than Semitone holds, so only its main outputs are on (" << p.getTotalNumOutputChannels() << " out)";
            if (count() > maxChannels) say << " - still too many, it can't play";
        }
        say << "; main " << p.getMainBusNumInputChannels() << " in, " << p.getMainBusNumOutputChannels() << " out";
        if (onLog) onLog (say);
    }
    bool scanning = false; juce::String scanningName; float progress = 0;

    PartHost()
    {
        fm.addDefaultFormats();
        if (auto xml = juce::XmlDocument::parse (listFile())) known.recreateFromXml (*xml);
    }
    ~PartHost() override
    {
        aliveFlag->store (false); stopTimer(); scanThread.reset();
        const AllHeld quiet (*this);
        bridgeWin.reset(); audition.reset();
        for (auto& t : fxWin) for (auto& w : t) w.reset();
        for (auto& t : fx) for (auto& p : t) p.reset();
        for (auto& w : windows) w.reset();
        for (auto& i : inst) i.reset();
    }

    void prepare (double rate, int blockSize)
    {
        // 25 Nov: not under the lock. Preparing can run the message loop, and a load finishing inside it would wait for that lock for ever.
        const AllHeld quiet (*this);
        sr = rate; bs = juce::jmax (64, blockSize); head.rate = rate;   // 64: the smallest block an export uses
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

    // mainOnly: every output but the main pair is switched off before it starts, as FL Studio does by default. Opus (21 Nov) stays silent with
    // all nine of its output pairs on ("channel not fully connected"); the page asks for this when a plug-in stays silent.
    void load (int ch, const juce::String& id, const juce::String& stateB64, bool mainOnly = false)
    {
        if (ch < 0 || ch > 15) return;
        clear (ch);
        const auto gen = ++loadGen[(size_t) ch];   // a later pick (or removing it) wins over a load still on its way
        if (id.isEmpty()) return;
        auto d = known.getTypeForIdentifierString (id);
        if (d == nullptr) { if (onLoaded) onLoaded (ch, false, "That plug-in isn't on this computer"); return; }
        ++loading;
        fm.createPluginInstanceAsync (*d, sr, bs, [this, ch, id, stateB64, gen, mainOnly, alive = aliveFlag] (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& err)
        {
            if (! alive->load()) return;
            --loading;
            if (gen != loadGen[(size_t) ch]) return;
            if (p == nullptr) { if (onLoaded) onLoaded (ch, false, err); return; }
            // leave its buses alone: some instruments crash if outputs are switched off. process() gives room for them all and uses the first two.
            p->setRateAndBufferSizeDetails (sr, bs);
            p->setPlayHead (&head);
            if (mainOnly) p->disableNonMainBuses();
            openMain (*p, false);
            if (stateB64.isNotEmpty()) { juce::MemoryBlock mb; if (mb.fromBase64Encoding (stateB64)) p->setStateInformation (mb.getData(), (int) mb.getSize()); }
            if (! alive->load() || gen != loadGen[(size_t) ch]) return;   // its own message boxes can let another load through
            openMain (*p, false);
            fitChannels (*p);
            p->prepareToPlay (sr, bs);
            const auto name = p->getName();
            instBad[(size_t) ch] = false; instSaid[(size_t) ch] = false; instReset[(size_t) ch] = 0;
            { const juce::SpinLock::ScopedLockType sl (lock); std::swap (inst[(size_t) ch], p); }
            instId[(size_t) ch] = id;
            if (onLoaded) onLoaded (ch, true, name);
        });
    }

    void clear (int ch)
    {
        windows[(size_t) ch].reset();
        std::unique_ptr<juce::AudioPluginInstance> old;
        { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (inst[(size_t) ch]); ccLast[(size_t) ch].fill (-1); }
        // 25 Nov: notes asked for and never played go with the plug-in, or they'd fill the queue for every other part
        for (auto& s : previewQueue) { int v = s.load(); if (v != 0 && (v >> 22) - 1 == ch) s.compare_exchange_strong (v, 0); }
        if (testCh.load() == ch) testCh = -1;
        instId[(size_t) ch].clear();
    }

    // Splice Bridge is an audition tool, not a part: it has no strip, isn't exported, and goes straight to the output.
    bool hasBridge() const { for (const auto& d : known.getTypes()) if (d.name == "Splice Bridge") return true; return false; }
    void bridgeLoad()
    {
        if (audition != nullptr) { if (onBridge) onBridge (true, audition->getName()); return; }
        std::unique_ptr<juce::PluginDescription> d;
        for (const auto& t : known.getTypes()) if (t.name == "Splice Bridge") { d = std::make_unique<juce::PluginDescription> (t); break; }
        if (d == nullptr) { if (onBridge) onBridge (false, "Splice Bridge isn't on this computer"); return; }
        ++loading;
        fm.createPluginInstanceAsync (*d, sr, bs, [this, alive = aliveFlag] (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& err)
        {
            if (! alive->load()) return;
            --loading;
            if (p == nullptr) { if (onBridge) onBridge (false, err); return; }
            p->setRateAndBufferSizeDetails (sr, bs);
            p->setPlayHead (&head);
            openMain (*p, false);
            fitChannels (*p);
            p->prepareToPlay (sr, bs);
            const auto name = p->getName();
            { const juce::SpinLock::ScopedLockType sl (lock); std::swap (audition, p); }
            bridgeWin.reset();   // a window of one it replaced
            if (onBridge) onBridge (true, name);
            bridgeOpen();
        });
    }
    void bridgeOpen()
    {
        if (bridgeWin) { bridgeWin->raise(); return; }
        if (audition == nullptr || ! audition->hasEditor()) return;
        bridgeWin = std::make_unique<HostWindow> (*audition, [this, alive = aliveFlag] { juce::MessageManager::callAsync ([this, alive] { if (alive->load()) bridgeWin.reset(); }); });
    }
    void bridgeClear()
    {
        bridgeWin.reset();
        std::unique_ptr<juce::AudioPluginInstance> old;
        { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (audition); }
        if (onBridge) onBridge (false, {});
    }
    void setLoop (double a, double b) { head.loopA = a; head.loopB = b; head.looping = b > a; }
    void setTransport (double bpm, double ppq, bool playing, int64_t samples) { head.bpm = bpm; head.ppq = ppq; head.playing = playing; head.samples = samples; }
    void setMeter (int top, int bot) { head.top = juce::jlimit (1, 32, top); head.bot = juce::jlimit (1, 32, bot); }

    // Audio thread, last of all: whatever Bridge is playing, straight to the output.
    void processAudition (float* L, float* R, int n)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || allHold.load() > 0 || audition == nullptr || n > temp.getNumSamples()) return;
        const int chans = juce::jmax (2, audition->getTotalNumOutputChannels(), audition->getTotalNumInputChannels());
        if (chans > maxChannels) return;
        temp.clear();
        juce::AudioBuffer<float> view (temp.getArrayOfWritePointers(), chans, n);
        juce::MidiBuffer none;
        audition->processBlock (view, none);
        if (! sane (view.getReadPointer (0), n) || ! sane (view.getReadPointer (1), n)) return;
        for (int i = 0; i < n; ++i) { L[i] += view.getSample (0, i); if (R != nullptr && R != L) R[i] += view.getSample (1, i); }
    }

    void openWindow (int ch)
    {
        if (ch < 0 || ch > 15) return;
        auto* p = inst[(size_t) ch].get();
        if (windows[(size_t) ch]) { windows[(size_t) ch]->raise(); return; }   // already open: bring it to the front
        if (p == nullptr || ! p->hasEditor()) return;
        windows[(size_t) ch] = std::make_unique<HostWindow> (*p, [this, ch, alive = aliveFlag] { juce::MessageManager::callAsync ([this, ch, alive] { if (alive->load()) windows[(size_t) ch].reset(); }); });
    }

    juce::String stateB64 (int ch)
    {
        if (ch < 0 || ch > 15) return {};
        auto* p = inst[(size_t) ch].get();
        if (p == nullptr) return {};
        juce::MemoryBlock mb; p->getStateInformation (mb);
        return mb.toBase64Encoding();
    }

    // message thread: which plug-in (or effect) is really there, so a saved sound only ever goes to its own plug-in
    juce::String loadedId (int ch) const { return ch >= 0 && ch < 16 ? instId[(size_t) ch] : juce::String(); }
    juce::String loadedFxId (int t, int slot) const { return t >= 0 && t <= masterFx && slot >= 0 && slot < fxSlots ? fxId[(size_t) t][(size_t) slot] : juce::String(); }

    void testNote (int ch) { if (ch >= 0 && ch < 16 && inst[(size_t) ch] != nullptr) testCh = ch; }

    // One note, right now, so a note placed on the piano roll is heard in the plug-in's own sound.
    void previewNote (int ch, int note, int vel, int ms)
    {
        if (ch < 0 || ch > 15 || inst[(size_t) ch] == nullptr) return;   // 25 Nov: with no plug-in there (it failed, or is still loading) nothing ever took the note out
        const int v = ((ch + 1) << 22) | (juce::jlimit (0, 127, note) << 15) | (juce::jlimit (1, 127, vel) << 8) | juce::jlimit (1, 255, ms / 10);
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

    std::function<void (int, int, bool, juce::String, bool)> onFxLoaded;   // target, slot, ok, name or why not, dry: it has no way in or out, so the sound goes past it
    std::function<void (int, int, juce::String)> onBad;                     // channel or target, slot (-1: the part's plug-in), name: it gave out something that isn't sound

    void fxLoad (int t, int slot, const juce::String& id, const juce::String& stateB64)
    {
        if (t < 0 || t > masterFx || slot < 0 || slot >= fxSlots) return;
        fxWin[(size_t) t][(size_t) slot].reset();
        std::unique_ptr<juce::AudioPluginInstance> old;
        { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (fx[(size_t) t][(size_t) slot]); }
        fxId[(size_t) t][(size_t) slot].clear();
        old.reset();
        const auto gen = ++fxGen[(size_t) t][(size_t) slot];
        if (id.isEmpty()) return;
        auto d = known.getTypeForIdentifierString (id);
        if (d == nullptr) { if (onFxLoaded) onFxLoaded (t, slot, false, "That effect isn't on this computer", false); return; }
        ++loading;
        fm.createPluginInstanceAsync (*d, sr, bs, [this, t, slot, id, stateB64, gen, alive = aliveFlag] (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& err)
        {
            if (! alive->load()) return;
            --loading;
            if (gen != fxGen[(size_t) t][(size_t) slot]) return;
            if (p == nullptr) { if (onFxLoaded) onFxLoaded (t, slot, false, err, false); return; }
            p->setRateAndBufferSizeDetails (sr, bs);
            openMain (*p, true);
            if (stateB64.isNotEmpty()) { juce::MemoryBlock mb; if (mb.fromBase64Encoding (stateB64)) p->setStateInformation (mb.getData(), (int) mb.getSize()); }
            if (! alive->load() || gen != fxGen[(size_t) t][(size_t) slot]) return;
            p->setPlayHead (&head);
            openMain (*p, true);
            fitChannels (*p);
            p->prepareToPlay (sr, bs);
            const auto name = p->getName();
            const bool dry = p->getMainBusNumInputChannels() == 0 || p->getMainBusNumOutputChannels() == 0;
            fxBad[(size_t) t][(size_t) slot] = false; fxSaid[(size_t) t][(size_t) slot] = false; fxReset[(size_t) t][(size_t) slot] = 0;
            { const juce::SpinLock::ScopedLockType sl (lock); std::swap (fx[(size_t) t][(size_t) slot], p); }
            fxId[(size_t) t][(size_t) slot] = id;
            if (onFxLoaded) onFxLoaded (t, slot, true, name, dry);
        });
    }

    // 12 Nov (Jamie: Space should play while a plug-in window is open): is one of our plug-in windows in front? The plug-in
    // takes the keys inside its own view, so the editor watches Space itself while this is true.
    bool hostWindowFront() const
    {
       #if JUCE_WINDOWS
        static juce::DynamicLibrary u ("user32.dll");
        using GetFg = void* (__stdcall*) ();
        using GetAnc = void* (__stdcall*) (void*, unsigned);
        auto fg = (GetFg) u.getFunction ("GetForegroundWindow");
        auto anc = (GetAnc) u.getFunction ("GetAncestor");
        if (fg == nullptr || anc == nullptr) return false;
        void* f = fg();
        if (f == nullptr) return false;
        void* root = anc (f, 2 /* GA_ROOT */);
        auto is = [&] (const std::unique_ptr<HostWindow>& w) { if (w == nullptr) return false; if (auto* p = w->getPeer()) { void* h = p->getNativeHandle(); return h == f || h == root; } return false; };
        if (is (bridgeWin)) return true;
        for (auto& w : windows) if (is (w)) return true;
        for (auto& row : fxWin) for (auto& w : row) if (is (w)) return true;
       #endif
        return false;
    }
    void fxOpen (int t, int slot)
    {
        if (t < 0 || t > masterFx || slot < 0 || slot >= fxSlots) return;
        auto* p = fx[(size_t) t][(size_t) slot].get();
        auto& w = fxWin[(size_t) t][(size_t) slot];
        if (w) { w->raise(); return; }
        if (p == nullptr || ! p->hasEditor()) return;
        w = std::make_unique<HostWindow> (*p, [this, t, slot, alive = aliveFlag] { juce::MessageManager::callAsync ([this, t, slot, alive] { if (alive->load()) fxWin[(size_t) t][(size_t) slot].reset(); }); });
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
        if (sl.isLocked() && allHold.load() == 0 && n <= fxTemp.getNumSamples()) runFx (t, L, R, n);
    }

    // Audio thread: the whole-song effects, after everything is mixed.
    void processMaster (float* L, float* R, int n)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (sl.isLocked() && allHold.load() == 0 && n <= fxTemp.getNumSamples()) runFx (masterFx, L, R, n);
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
        const Held held (*this, instHold[(size_t) ch]);
        p->setCurrentProgram (i);
    }

    void setState (int ch, const juce::String& b64)
    {
        auto* p = (ch >= 0 && ch < 16) ? inst[(size_t) ch].get() : nullptr;
        juce::MemoryBlock mb;
        if (p == nullptr || b64.isEmpty() || ! mb.fromBase64Encoding (b64)) return;
        // 25 Nov: only this plug-in waits. Under the lock every plug-in and effect stopped until it was done, note-offs were lost, and one
        // that put up a message box could freeze the app.
        const Held held (*this, instHold[(size_t) ch]);
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
        if (beat < 0 && rolling) stopFor.fill (true);
        rolling = beat >= 0;
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || allHold.load() > 0 || n > temp.getNumSamples() || n > partOut.getNumSamples()) { keepOffs (events, -1); return; }
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
                        else if (k.fxParam >= 0 && k.fxParam < ps.size() && instHold[(size_t) k.ch].load() == 0) ps[k.fxParam]->setValue (v);
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
            auto& keys = down[(size_t) ch];
            if (p == nullptr) { keys.reset(); pendOff[(size_t) ch].reset(); pendBend[(size_t) ch] = -1; stopFor[(size_t) ch] = false; continue; }
            if (instHold[(size_t) ch].load() > 0 || instBad[(size_t) ch].load()) { keepOffs (events, ch); continue; }
            midi.clear();
            // notes that ended while this plug-in was held back end now, so none is left hanging
            if (pendOff[(size_t) ch].any())
            {
                for (int k = 0; k < 128; ++k)
                    if (pendOff[(size_t) ch].test ((size_t) k)) { midi.addEvent (juce::MidiMessage (0x80, k, 0), 0); keys.reset ((size_t) k); }
                pendOff[(size_t) ch].reset();
            }
            if (pendBend[(size_t) ch] >= 0) { midi.addEvent (juce::MidiMessage (0xE0, pendBend[(size_t) ch] & 127, pendBend[(size_t) ch] >> 7), 0); pendBend[(size_t) ch] = -1; }
            // stopped: end whatever is still down, and say so with all-notes-off (123). Not all-sound-off (120): that would cut every tail dead.
            if (stopFor[(size_t) ch])
            {
                stopFor[(size_t) ch] = false;
                if (keys.any())
                    for (int k = 0; k < 128; ++k)
                        if (keys.test ((size_t) k)) midi.addEvent (juce::MidiMessage (0x80, k, 0), 0);
                keys.reset();
                midi.addEvent (juce::MidiMessage (0xB0, 123, 0), 0);
            }
            for (int q = 0; q < ccCount; ++q)
                if (ccOut[(size_t) q].ch == ch) midi.addEvent (juce::MidiMessage::controllerEvent (1, ccOut[(size_t) q].cc, ccOut[(size_t) q].v), 0);
            for (const auto& e : events)
                if ((e.status & 15) == ch)
                {
                    midi.addEvent (juce::MidiMessage (e.status & 0xF0, e.d1, e.d2), e.offset);
                    const int type = e.status & 0xF0;
                    if (type == 0x90 && e.d2 > 0) keys.set ((size_t) (e.d1 & 127));
                    else if (type == 0x80 || type == 0x90) keys.reset ((size_t) (e.d1 & 127));
                }
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
            if (p->getMainBusNumOutputChannels() == 1) view.copyFrom (1, 0, view, 0, 0, n);   // 25 Nov: a mono instrument was only on the left
            if (! sane (view.getReadPointer (0), n) || ! sane (view.getReadPointer (1), n))
            {
                view.clear (0, 0, n); view.clear (1, 0, n);
                instBad[(size_t) ch] = true;
            }
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
        const juce::SpinLock::ScopedLockType sl (lock);   // called from the export's thread
        for (auto& p : inst) if (p) p->setNonRealtime (on);
        for (auto& t : fx) for (auto& p : t) if (p) p->setNonRealtime (on);
    }

    int loadsPending() const { return loading; }             // message thread: plug-ins and effects still on their way

    // Message thread, a couple of times a second: a plug-in that gave out something that isn't sound (25 Nov: one bad sample silenced the whole
    // song until a restart) has been left out since. Start it afresh, at most every 5 seconds, and say so once.
    void pollBad()
    {
        const auto now = juce::jmax ((juce::uint32) 1, juce::Time::getMillisecondCounter());
        auto again = [now] (juce::uint32& last) { if (last != 0 && now - last < 5000) return false; last = now; return true; };
        for (size_t c = 0; c < inst.size(); ++c)
            if (instBad[c].load() && inst[c] != nullptr && again (instReset[c]))
            {
                inst[c]->reset();
                instBad[c] = false;
                if (! instSaid[c]) { instSaid[c] = true; if (onBad) onBad ((int) c, -1, inst[c]->getName()); }
            }
        for (size_t t = 0; t < fx.size(); ++t)
            for (size_t s = 0; s < fx[t].size(); ++s)
                if (fxBad[t][s].load() && fx[t][s] != nullptr && again (fxReset[t][s]))
                {
                    fx[t][s]->reset();
                    fxBad[t][s] = false;
                    if (! fxSaid[t][s]) { fxSaid[t][s] = true; if (onBad) onBad ((int) t, (int) s, fx[t][s]->getName()); }
                }
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
                        // in short waits: closing the app used to wait up to a minute for the plug-in being looked at
                        bool ended = false;
                        for (int w = 0; w < 300 && ! ended && ! threadShouldExit(); ++w) ended = p.waitForProcessToFinish (200);
                        if (! ended) p.kill();
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
    int loading = 0;                                               // message thread
    // 25 Nov: a plug-in whose sound is being changed (or all of them, while they are prepared) sits out. Setting the flag and then taking the
    // lock once means the audio thread is out of it; after that it looks at the flag first.
    std::atomic<int> allHold { 0 };
    std::array<std::atomic<int>, 16> instHold {};
    struct Held
    {
        Held (PartHost& h, std::atomic<int>& f) : flag (f) { flag.fetch_add (1); const juce::SpinLock::ScopedLockType sl (h.lock); }
        ~Held() { flag.fetch_sub (1); }
        std::atomic<int>& flag;
        JUCE_DECLARE_NON_COPYABLE (Held)
    };
    struct AllHeld : Held { explicit AllHeld (PartHost& h) : Held (h, h.allHold) {} };
    // audio thread sets, pollBad clears: gave out something that isn't sound
    std::array<std::atomic<bool>, 16> instBad {};
    std::array<std::array<std::atomic<bool>, fxSlots>, masterFx + 1> fxBad {};
    std::array<bool, 16> instSaid {};
    std::array<std::array<bool, fxSlots>, masterFx + 1> fxSaid {};
    std::array<juce::uint32, 16> instReset {};
    std::array<std::array<juce::uint32, fxSlots>, masterFx + 1> fxReset {};
    // audio thread: the notes each plug-in has down, the note-offs and bend that came while it sat out, and who still has to hear about a stop
    std::array<std::bitset<128>, 16> down, pendOff;
    std::array<int, 16> pendBend = [] { std::array<int, 16> a {}; a.fill (-1); return a; }();
    std::array<bool, 16> stopFor {};
    bool rolling = false;
    void keepOffs (const std::vector<Event>& events, int ch)
    {
        for (const auto& e : events)
        {
            const int c = e.status & 15, type = e.status & 0xF0;
            if (ch >= 0 && c != ch) continue;
            if (type == 0x80 || (type == 0x90 && e.d2 == 0)) pendOff[(size_t) c].set ((size_t) (e.d1 & 127));
            else if (type == 0xE0) pendBend[(size_t) c] = (e.d1 & 127) | ((e.d2 & 127) << 7);
        }
    }
    // real sound: no NaN, no infinity, nothing absurd (a million times full scale)
    static bool sane (const float* x, int n)
    {
        static_assert (sizeof (float) == sizeof (uint32_t), "32-bit floats");
        uint32_t most = 0;
        for (int i = 0; i < n; ++i)
        {
            uint32_t u;
            std::memcpy (&u, x + i, sizeof (u));
            most = std::max (most, u & 0x7fffffffu);
        }
        return most < 0x49742400u;     // the bits of 1000000.0f
    }
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
        for (size_t s = 0; s < fx[(size_t) t].size(); ++s)
        {
            auto* p = fx[(size_t) t][s].get();
            if (p == nullptr || fxBad[(size_t) t][s].load()) continue;
            const int in = p->getMainBusNumInputChannels(), out = p->getMainBusNumOutputChannels();
            if (in == 0 || out == 0) continue;     // 25 Nov: no way in or no way out: the sound goes past it (the page is told)
            const bool two = R != nullptr && R != L;
            const bool mid = in == 1 && two;       // a mono effect gets the middle of the stereo sound
            const int chans = juce::jmax (2, p->getTotalNumOutputChannels(), p->getTotalNumInputChannels());
            if (chans > maxChannels) continue;
            juce::AudioBuffer<float> view (fxTemp.getArrayOfWritePointers(), chans, n);
            view.clear();
            if (mid)
            {
                auto* m = view.getWritePointer (0);
                for (int i = 0; i < n; ++i) m[i] = .5f * (L[i] + R[i]);
            }
            else
            {
                view.copyFrom (0, 0, L, n);
                if (in > 1) view.copyFrom (1, 0, R != nullptr ? R : L, n);
            }
            juce::MidiBuffer none;
            p->processBlock (view, none);
            const float* a = view.getReadPointer (0);
            const float* b = view.getReadPointer (out > 1 ? 1 : 0);
            if (! sane (a, n) || ! sane (b, n)) { fxBad[(size_t) t][s] = true; continue; }   // the sound goes past it until pollBad has reset it
            if (mid && out == 1 && p->getLatencySamples() == 0)
            {
                // mono in, mono out: the sides go round it, so the part keeps its width (not when it answers late: they'd no longer line up)
                for (int i = 0; i < n; ++i) { const float side = .5f * (L[i] - R[i]); L[i] = a[i] + side; R[i] = a[i] - side; }
                continue;
            }
            juce::FloatVectorOperations::copy (L, a, n);
            if (two) juce::FloatVectorOperations::copy (R, b, n);
        }
    }

    std::array<std::array<std::unique_ptr<juce::AudioPluginInstance>, 4>, 17> fx;
    std::array<std::array<std::unique_ptr<HostWindow>, 4>, 17> fxWin;
    std::array<juce::String, 16> instId;
    std::array<std::array<juce::String, 4>, 17> fxId;
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
    static constexpr int maxChannels = 256;   // 21 Nov: was 128; multi-out instruments (Opus) can ask for more
    juce::AudioBuffer<float> temp { maxChannels, 8192 };
    juce::MidiBuffer midi;
};
}
