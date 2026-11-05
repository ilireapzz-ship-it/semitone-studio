#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Sequencer.h"
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
    ~PartHost() override { stopTimer(); const juce::SpinLock::ScopedLockType sl (lock); for (auto& w : windows) w.reset(); for (auto& i : inst) i.reset(); }

    void prepare (double rate, int blockSize)
    {
        sr = rate; bs = juce::jmax (32, blockSize);
        const juce::SpinLock::ScopedLockType sl (lock);
        for (auto& i : inst) if (i) { i->releaseResources(); i->setRateAndBufferSizeDetails (sr, bs); i->prepareToPlay (sr, bs); }
        temp.setSize (maxChannels, bs * 2, false, false, true);
    }

    void startScan()
    {
        if (scanning) return;
        scanning = true; fmtIndex = 0; progress = 0; nextFormat(); startTimer (5);
    }

    juce::var listVar() const
    {
        juce::Array<juce::var> a;
        for (const auto& d : known.getTypes())
        {
            if (! d.isInstrument) continue;
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
            // just the main stereo output: big instruments offer dozens of extra outputs and side inputs
            for (int i = p->getBusCount (true) - 1; i >= 0; --i) p->getBus (true, i)->enable (false);
            for (int i = p->getBusCount (false) - 1; i >= 1; --i) p->getBus (false, i)->enable (false);
            if (auto* main = p->getBus (false, 0)) main->enable (true);
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
        { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (inst[(size_t) ch]); }
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

    bool hasAny() const { for (const auto& i : inst) if (i) return true; return false; }

    // Audio thread: skips the block if a plug-in is being swapped.
    void process (const std::vector<Event>& events, int n, float* L, float* R)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || n > temp.getNumSamples()) return;
        for (int ch = 0; ch < 16; ++ch)
        {
            auto* p = inst[(size_t) ch].get();
            if (p == nullptr) continue;
            midi.clear();
            for (const auto& e : events)
                if ((e.status & 15) == ch) midi.addEvent (juce::MidiMessage (e.status & 0xF0, e.d1, e.d2), e.offset);
            if (testCh.load() == ch) { testCh = -1; heldCh = ch; heldLeft = (int) (sr * .7); midi.addEvent (juce::MidiMessage (0x90, 60, 100), 0); }
            if (heldCh == ch && (heldLeft -= n) <= 0) { heldCh = -1; midi.addEvent (juce::MidiMessage (0x80, 60, 0), 0); }
            const int outs = juce::jlimit (2, maxChannels, juce::jmax (p->getTotalNumOutputChannels(), p->getTotalNumInputChannels()));
            juce::AudioBuffer<float> view (temp.getArrayOfWritePointers(), outs, n);
            view.clear();
            p->processBlock (view, midi);
            juce::FloatVectorOperations::add (L, view.getReadPointer (0), n);
            if (R != nullptr && R != L) juce::FloatVectorOperations::add (R, view.getReadPointer (1), n);
        }
    }

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
    int heldCh = -1, heldLeft = 0;
    static constexpr int maxChannels = 128;
    juce::AudioBuffer<float> temp { maxChannels, 8192 };
    juce::MidiBuffer midi;
};
}
