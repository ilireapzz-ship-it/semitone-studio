// Kick Fit: the plug-in and its window. The sound work is in KickFit.h; the genre targets in KickFitTargets.h
// (made by tools/kickfit_targets.py from data/kickfit-targets.json, which holds measurements, not guesses).
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "KickFit.h"
#include "FitLink.h"
#include <random>
#include "KickFitTargets.h"

namespace
{
const char* const noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
juce::String noteText (double hz)
{
    if (hz <= 0) return "?";
    const double m = 69 + 12 * std::log2 (hz / 440.0); const int r = (int) std::lround (m), cents = (int) std::lround (100 * (m - r));
    return juce::String (noteNames[((r % 12) + 12) % 12]) + juce::String (r / 12 - 1) + (cents ? juce::String (cents > 0 ? " +" : " ") + juce::String (cents) + " cents" : juce::String());
}
constexpr int scopeSize = 16384, fftOrder = 13, fftSize = 1 << fftOrder;

struct Scope       // the last stretch of sound, written by the audio thread, read by the window
{
    std::array<float, scopeSize> data {}; std::atomic<int> w { 0 };
    void push (float v) { const int i = w.load (std::memory_order_relaxed); data[(size_t) i] = v; w.store ((i + 1) & (scopeSize - 1), std::memory_order_relaxed); }
    void latest (float* out, int n) const { const int e = w.load(); for (int i = 0; i < n; ++i) out[i] = data[(size_t) ((e - n + i) & (scopeSize - 1))]; }
};
// Two Kick Fits in one song work as a pair: one on the kick, one on the bass with the kick on its sidechain.
// (FL Studio won't let two mixer tracks feed each other's sidechains, so only the one on the bass hears both.)
std::atomic<float> linkedKickHz { 0.0f };       // the one on the kick says where the boom is
std::atomic<float> linkedRoom { -1.0f };        // and how much room the bass should make (-1 = no Kick Fit on a kick)
juce::CriticalSection linkLock;                 // held while the window reads the other one, and while one is deleted
}

class KickFitProcessor;
namespace { std::atomic<KickFitProcessor*> bassOne { nullptr }, kickOne { nullptr }; }

class KickFitProcessor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::Timer
{
public:
    ~KickFitProcessor() override
    {
        stopTimer(); fl::release (fl::shared(), linkSlot.load(), linkId);
        const juce::ScopedLock sl (linkLock);
        KickFitProcessor* me = this; bassOne.compare_exchange_strong (me, nullptr);
        me = this; if (kickOne.compare_exchange_strong (me, nullptr)) linkedRoom.store (-1.0f);
    }
    KickFitProcessor()
        : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                           .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
          state (*this, nullptr, "KickFit", layout())
    {
        for (auto* id : { "role", "mode", "key", "thump", "punch", "click", "tail", "room", "tune", "level" }) raw[id] = state.getRawParameterValue (id);
        std::random_device rd; linkId = (uint32_t) rd() | 1u; startTimerHz (10);
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout()
    {
        using F = juce::AudioParameterFloat; using C = juce::AudioParameterChoice;
        juce::StringArray modes, keys { "Listen for it" };
        for (auto& m : kf::modeTargets) modes.add (m.name);
        for (int minor = 0; minor < 2; ++minor) for (auto* n : noteNames) keys.add (juce::String (n) + (minor ? " minor" : " major"));
        juce::AudioProcessorValueTreeState::ParameterLayout l;
        l.add (std::make_unique<C> (juce::ParameterID { "role", 1 }, "I am on", juce::StringArray { "The kick", "The bass", "The vocal", "Everything else" }, 0));
        l.add (std::make_unique<C> (juce::ParameterID { "mode", 1 }, "Genre", modes, 0));
        l.add (std::make_unique<C> (juce::ParameterID { "key", 1 }, "Key", keys, 0));
        for (auto* id : { "thump", "punch", "click", "tail" })
            l.add (std::make_unique<F> (juce::ParameterID { id, 1 }, juce::String (id).substring (0, 1).toUpperCase() + juce::String (id).substring (1), juce::NormalisableRange<float> (-100, 100, 1), 0.0f));
        l.add (std::make_unique<F> (juce::ParameterID { "room", 1 }, "Room for bass", juce::NormalisableRange<float> (0, 100, 1), 0.0f));
        l.add (std::make_unique<F> (juce::ParameterID { "tune", 1 }, "Tune", juce::NormalisableRange<float> (-12, 12, 0.01f), 0.0f));
        l.add (std::make_unique<F> (juce::ParameterID { "level", 1 }, "Level", juce::NormalisableRange<float> (-24, 24, 0.1f), 0.0f));
        return l;
    }

    const juce::String getName() const override { return "Kick Fit"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet(), sc = l.getChannelSet (true, 1);
        if (in != out || (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())) return false;
        return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
    }
    void prepareToPlay (double sr, int block) override
    {
        rate = sr; engine.setParams (read()); engine.prepare (sr); linkMeter.prepare (sr); linkUsual.prepare (sr);
        spare.setSize (1, std::max (block, 4096)); setLatencySamples (engine.latency()); lastLatency = engine.latency();
    }
    void releaseResources() override {}

    kf::Params read() const
    {
        kf::Params p;
        p.role = (int) raw.at ("role")->load();
        p.thump = raw.at ("thump")->load() / 100; p.punch = raw.at ("punch")->load() / 100; p.click = raw.at ("click")->load() / 100; p.tail = raw.at ("tail")->load() / 100;
        p.room = raw.at ("room")->load() / 100; p.tune = raw.at ("tune")->load(); p.gainDb = raw.at ("level")->load();
        const float k = linkedKickHz.load();                       // the dip covers the kick's boom and the octave above it
        p.duckHz = k > 0 ? juce::jlimit (90.0f, 200.0f, k * 2.2f) : 120.0f;
        const float lr = linkedRoom.load(); if (p.role == 1 && lr >= 0) p.room = lr;        // the knob on the kick's Kick Fit leads
        return p;
    }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        auto main = getBusBuffer (buffer, true, 0);
        const int n = main.getNumSamples();
        if (n == 0 || main.getNumChannels() == 0) return;
        const float *sl = nullptr, *sr = nullptr;
        if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
        {
            auto side = getBusBuffer (buffer, true, 1);
            if (side.getNumChannels() > 0) { sl = side.getReadPointer (0); sr = side.getReadPointer (side.getNumChannels() > 1 ? 1 : 0); }
        }
        const auto p = read();
        engine.setParams (p);
        float* l = main.getWritePointer (0); float* r = l;
        if (main.getNumChannels() > 1) r = main.getWritePointer (1);
        else { if (spare.getNumSamples() < n) return; spare.copyFrom (0, 0, l, n); r = spare.getWritePointer (0); }        // mono: the same sound on both sides
        if (sl != nullptr)          // FL Studio hands an unconnected sidechain a copy of this track's own sound: that is not a sidechain
        {
            bool same = true;
            for (int i = 0; i < n && same; ++i) same = sl[i] == l[i] && sr[i] == r[i];
            if (same) sl = sr = nullptr;
        }
        engine.process (l, r, sl, sr, n);
        for (int i = 0; i < n; ++i) { mine.push (0.5f * (l[i] + r[i])); other.push (sl ? 0.5f * (sl[i] + sr[i]) : 0.0f); }
        tellBus (l, r, n, p);
        if (p.role == 1) { if (bassOne.load (std::memory_order_relaxed) != this) bassOne.store (this); }
        else if (p.role == 0) { if (kickOne.load (std::memory_order_relaxed) != this) kickOne.store (this); linkedRoom.store (raw.at ("room")->load() / 100); }
        if (p.role == 0 && engine.note() > 0) linkedKickHz.store ((float) (engine.note() * kf::Engine::ratioOf (p.tune)));
        if (engine.latency() != lastLatency) { lastLatency = engine.latency(); triggerAsyncUpdate(); }
    }
    void handleAsyncUpdate() override { setLatencySamples (lastLatency); }

    // Fit Bus (a separate plug-in on the bus) reads what this Kick Fit is on and how strong its output is: see FitLink.h
    void tellBus (const float* l, const float* r, int n, const kf::Params& p)
    {
        auto* b = fl::shared(); if (b == nullptr) return;
        for (int i = 0; i < n; ++i)
        {
            const double size = std::max (std::abs (l[i]), std::abs (r[i])); linkUsual.push (size);
            if (! linkMeter.push (0.5 * ((double) l[i] + r[i]), size)) continue;
            int s = linkSlot.load (std::memory_order_relaxed);
            if (s < 0 || b->slot[s].owner.load() != linkId)
            {
                s = fl::claim (b, linkId); linkSlot.store (s); if (s < 0) return;
                linkSeq.store (b->slot[s].askSeq.load());               // requests left for whoever had the slot before are not ours
            }
            auto& t = b->slot[s];
            t.role.store (p.role); for (int k = 0; k < fl::numBands; ++k) t.power[k].store ((float) linkMeter.power[k]);
            t.peak.store ((float) linkMeter.peak); t.usual.store ((float) linkUsual.value); t.level.store (p.gainDb); t.seenMs.store (fl::nowMs());
        }
    }
    void timerCallback() override           // Fit Bus asked for a Level change (its Fix button)
    {
        auto* b = fl::shared(); const int s = linkSlot.load(); if (b == nullptr || s < 0 || b->slot[s].owner.load() != linkId) return;
        const uint32_t seq = b->slot[s].askSeq.load(); if (seq == linkSeq.load()) return;
        linkSeq.store (seq); const float ask = b->slot[s].askDb.load();
        if (std::isfinite (ask) && std::abs (ask) <= 48.0f) setParam ("level", juce::jlimit (-24.0f, 24.0f, raw.at ("level")->load() + ask));
    }

    void getStateInformation (juce::MemoryBlock& dest) override { if (auto xml = state.copyState().createXml()) copyXmlToBinary (*xml, dest); }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size)) if (xml->hasTagName (state.state.getType())) state.replaceState (juce::ValueTree::fromXml (*xml));
    }
    void setParam (const char* id, float value)
    {
        if (auto* p = state.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (value)); p->endChangeGesture(); }
    }
    float get (const char* id) const { return raw.at (id)->load(); }

    juce::AudioProcessorValueTreeState state;
    kf::Engine engine; Scope mine, other; double rate = 48000;
private:
    std::map<juce::String, std::atomic<float>*> raw; juce::AudioBuffer<float> spare; int lastLatency = 0;
    fl::BandMeter linkMeter; fl::UsualPeak linkUsual; std::atomic<int> linkSlot { -1 }; std::atomic<uint32_t> linkSeq { 0 }; uint32_t linkId = 1;
};

// ---------- the window ----------
namespace look
{
const juce::Colour paper (0xff121314), panel (0xff1b1d1f), line (0xff2c2f33), ink (0xffe9e6df), soft (0xff9a968d), kick (0xffe8a23a), bass (0xff5fb0c9), zone (0xff6fbf73);
}

class KickFitEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KickFitEditor (KickFitProcessor& p) : AudioProcessorEditor (p), proc (p), fft (fftOrder), window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        for (auto* c : { &role, &mode, &key }) { addAndMakeVisible (*c); c->setColour (juce::ComboBox::backgroundColourId, look::panel); c->setColour (juce::ComboBox::textColourId, look::ink); c->setColour (juce::ComboBox::outlineColourId, look::line); }
        auto fill = [&] (juce::ComboBox& box, const char* id) { if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.state.getParameter (id))) box.addItemList (ch->choices, 1); boxes.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (proc.state, id, box)); };
        fill (role, "role"); fill (mode, "mode"); fill (key, "key");
        const char* ids[] = { "thump", "punch", "click", "tail", "room", "tune", "level" };
        const char* names[] = { "Thump", "Punch", "Click", "Tail", "Room for bass", "Tune", "Level" };
        for (int i = 0; i < 7; ++i)
        {
            auto* s = knobs.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
            s->setColour (juce::Slider::rotarySliderFillColourId, i == 4 ? look::bass : look::kick); s->setColour (juce::Slider::rotarySliderOutlineColourId, look::line);
            s->setColour (juce::Slider::thumbColourId, look::ink); s->setColour (juce::Slider::textBoxTextColourId, look::ink); s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
            addAndMakeVisible (s); sliders.add (new juce::AudioProcessorValueTreeState::SliderAttachment (proc.state, ids[i], *s));
            auto* l = labels.add (new juce::Label ({}, names[i])); l->setJustificationType (juce::Justification::centred); l->setColour (juce::Label::textColourId, look::soft); l->setFont (juce::FontOptions (13.0f)); addAndMakeVisible (l);
        }
        knobs[5]->setTextValueSuffix (" st"); knobs[6]->setTextValueSuffix (" dB");
        for (auto* b : { &tuneBtn, &sitBtn }) { addAndMakeVisible (*b); b->setColour (juce::TextButton::buttonColourId, look::panel); b->setColour (juce::TextButton::textColourOffId, look::ink); }
        sitBtn.setButtonText ("Sit it");
        sitBtn.onClick = [this] { sit(); };
        tuneBtn.onClick = [this] { if (canTune) proc.setParam ("tune", (float) tuneMove); };
        said.setColour (juce::Label::textColourId, look::ink); said.setFont (juce::FontOptions (13.5f)); said.setJustificationType (juce::Justification::centredLeft); addAndMakeVisible (said);
        setSize (820, 500);          // last: sizing lays out the knobs, so they must exist first
        startTimerHz (24);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (12);
        auto top = a.removeFromTop (28);
        top.removeFromLeft (110);
        role.setBounds (top.removeFromLeft (120)); top.removeFromLeft (8); mode.setBounds (top.removeFromLeft (190)); top.removeFromLeft (8); key.setBounds (top.removeFromLeft (150));
        a.removeFromTop (8);
        auto bottom = a.removeFromBottom (128);
        said.setBounds (a.removeFromBottom (40));
        picture = a;
        auto btns = bottom.removeFromRight (150).reduced (4, 14);
        sitBtn.setBounds (btns.removeFromTop (40)); btns.removeFromTop (8); tuneBtn.setBounds (btns.removeFromTop (40));
        const int w = bottom.getWidth() / 7;
        for (int i = 0; i < knobs.size() && i < labels.size(); ++i) { auto c = bottom.removeFromLeft (w); labels[i]->setBounds (c.removeFromTop (18)); knobs[i]->setBounds (c.reduced (4, 0)); }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (look::paper);
        g.setColour (look::ink); g.setFont (juce::FontOptions (19.0f, juce::Font::bold)); g.drawText ("Kick Fit", 12, 12, 110, 28, juce::Justification::centredLeft);
        auto a = picture;
        auto timeBox = a.removeFromRight (a.getWidth() * 34 / 100); a.removeFromRight (8);
        auto meterBox = a.removeFromBottom (46); a.removeFromBottom (6);
        drawPitch (g, a); drawTime (g, timeBox); drawMeter (g, meterBox);
    }

private:
    static float xOf (double hz, juce::Rectangle<int> r) { return r.getX() + (float) (std::log (hz / 20.0) / std::log (500.0 / 20.0)) * r.getWidth(); }
    void frame (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (look::panel); g.fillRoundedRectangle (r.toFloat(), 6); g.setColour (look::line); g.drawRoundedRectangle (r.toFloat(), 6, 1);
        g.setColour (look::soft); g.setFont (juce::FontOptions (12.0f)); g.drawText (title, r.reduced (8, 4), juce::Justification::topLeft);
    }
    void drawPitch (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const int role = (int) proc.get ("role"); const bool onBass = role == 1;
        frame (g, r, role == 3 ? "Pitch: everything else in the song" : role == 2 ? "Pitch: this vocal (orange) and the rest of the song (blue)" : onBass ? "Pitch: this bass (blue) and the kick on the sidechain (orange)" : paired ? "Pitch: this kick (orange) and the bass (blue, from the Kick Fit on the bass)" : "Pitch: this kick (orange) and the sidechain (blue)");
        auto in = r.reduced (8, 22);
        const auto& t = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.get ("mode"))];
        if (role >= 2) {}
        else if (t.measured && t.noteHi > t.noteLo)
        {
            const float x0 = xOf (t.noteLo, in), x1 = xOf (t.noteHi, in);
            g.setColour (look::zone.withAlpha (0.16f)); g.fillRect (x0, (float) in.getY(), x1 - x0, (float) in.getHeight());
            g.setColour (look::zone); g.setFont (juce::FontOptions (11.0f)); g.drawText (juce::String ("where the boom sits in your ") + t.name + " songs", (int) x0 + 4, in.getY() + 2, 260, 14, juce::Justification::left);
        }
        else { g.setColour (look::soft); g.setFont (juce::FontOptions (11.0f)); g.drawText (juce::String ("No zone: not enough of your ") + t.name + " songs to measure", in.removeFromTop (14), juce::Justification::right); }
        g.setFont (juce::FontOptions (10.0f));
        for (int m = 24; m <= 71; ++m)
        {
            const double hz = 440 * std::pow (2.0, (m - 69) / 12.0); if (hz < 20 || hz > 500) continue;
            const float x = xOf (hz, in);
            if (m % 12 == 0) { g.setColour (look::line); g.drawVerticalLine ((int) x, (float) in.getY(), (float) in.getBottom()); g.setColour (look::soft); g.drawText ("C" + juce::String (m / 12 - 1), (int) x + 2, in.getBottom() - 12, 30, 12, juce::Justification::left); }
        }
        auto curve = [&] (const std::vector<float>& mag, juce::Colour c, bool filled)
        {
            juce::Path p; bool started = false; const double binHz = proc.rate / fftSize;
            for (int x = 0; x <= in.getWidth(); x += 2)
            {
                const double hz = 20.0 * std::pow (500.0 / 20.0, x / (double) in.getWidth()), bin = hz / binHz; const int i = (int) bin;
                if (i + 1 >= (int) mag.size()) break;
                const float v = mag[(size_t) i] + (float) (bin - i) * (mag[(size_t) i + 1] - mag[(size_t) i]);
                const float y = in.getBottom() - juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -90.0f) + 72) / 72) * in.getHeight();
                if (! started) { p.startNewSubPath ((float) in.getX() + x, y); started = true; } else p.lineTo ((float) in.getX() + x, y);
            }
            if (! started) return;
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.6f));
            if (filled) { p.lineTo ((float) in.getRight(), (float) in.getBottom()); p.lineTo ((float) in.getX(), (float) in.getBottom()); p.closeSubPath(); g.setColour (c.withAlpha (0.18f)); g.fillPath (p); }
        };
        curve (onBass ? magMine : magOther, look::bass, false);
        curve (onBass ? magOther : magMine, look::kick, true);
        if (role == 0 && shownHz > 20 && shownHz < 500)
        {
            const float x = xOf (shownHz, in);
            g.setColour (look::ink); g.drawVerticalLine ((int) x, (float) in.getY() + 16, (float) in.getBottom());
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold)); g.drawText (noteText (shownHz), (int) x + 4, in.getY() + 18, 120, 14, juce::Justification::left);
        }
    }
    void drawTime (juce::Graphics& g, juce::Rectangle<int> r)
    {
        frame (g, r, proc.get ("role") > 1.5f ? "Level: the last 50 seconds" : "Time: one hit, lows only (0.4 s)");
        auto in = r.reduced (8, 22); const juce::ScopedLock sl (linkLock); const auto& v = ctx().view();
        float top = 1e-4f; for (int i = 0; i < kf::HitView::n; ++i) top = std::max ({ top, v.kick[i], v.side[i] });
        auto draw = [&] (const float* d, juce::Colour c, bool filled)
        {
            juce::Path p; p.startNewSubPath ((float) in.getX(), (float) in.getBottom());
            for (int i = 0; i < kf::HitView::n; ++i) p.lineTo (in.getX() + in.getWidth() * (i + 0.5f) / kf::HitView::n, in.getBottom() - std::sqrt (d[i] / top) * in.getHeight());
            p.lineTo ((float) in.getRight(), (float) in.getBottom());
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.5f)); if (filled) { g.setColour (c.withAlpha (0.18f)); g.fillPath (p); }
        };
        draw (v.side, look::bass, false); draw (v.kick, look::kick, true);           // on the bass the sidechain is the kick: the engine already files it under kick
    }
    void drawMeter (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const auto& tv = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.get ("mode"))];
        if (proc.get ("role") > 2.5f) { frame (g, r, "Nothing to sit here: Fit Bus reads this part on the bus"); return; }
        if (proc.get ("role") > 1.5f)
        {
            frame (g, r, "Vocal against everything else");
            auto bar = r.reduced (10, 0).removeFromBottom (18).translated (0, -6);
            auto x = [&] (double dB) { return bar.getX() + (float) juce::jlimit (0.0, 1.0, (dB + 15) / 25) * bar.getWidth(); };
            g.setColour (look::paper); g.fillRect (bar);
            if (tv.vocal) { g.setColour (look::zone.withAlpha (0.35f)); g.fillRect (x (tv.vocLo), (float) bar.getY(), x (tv.vocHi) - x (tv.vocLo), (float) bar.getHeight()); }
            double now; if (proc.engine.hasSidechain() && proc.engine.vocalLevel (now)) { g.setColour (look::kick); g.fillRect (x (now) - 2, (float) bar.getY() - 3, 4.0f, (float) bar.getHeight() + 6); }
            return;
        }
        frame (g, r, "Kick's lows against all the lows");
        if (proc.get ("role") > 0.5f) { g.setColour (look::soft); g.drawText (juce::String ("Bass lows now: ") + juce::String (proc.engine.duckNowDb(), 1) + " dB", r.reduced (8, 4), juce::Justification::centredRight); return; }
        auto bar = r.reduced (10, 0).removeFromBottom (18).translated (0, -6);
        const auto& t = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.get ("mode"))];
        auto x = [&] (double dB) { return bar.getX() + (float) juce::jlimit (0.0, 1.0, (dB + 30) / 30) * bar.getWidth(); };
        g.setColour (look::paper); g.fillRect (bar);
        if (t.measured) { g.setColour (look::zone.withAlpha (0.35f)); g.fillRect (x (t.sitLo), (float) bar.getY(), x (t.sitHi) - x (t.sitLo), (float) bar.getHeight()); }
        const juce::ScopedLock sl (linkLock); double now; if (ctx().hasSidechain() && ctx().sitLevel (now)) { g.setColour (look::kick); g.fillRect (x (now) - 2, (float) bar.getY() - 3, 4.0f, (float) bar.getHeight() + 6); }
    }

    void sit()
    {
        const auto& t = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.get ("mode"))];
        double change = 0, now = 0;
        if (proc.get ("role") > 1.5f)
        {
            if (! t.vocal) { sitSaid = juce::String ("Not enough of your ") + t.name + " songs were measured to know where the vocal should sit. Pick another genre or set Level by ear."; return; }
            if (! proc.engine.hasSidechain()) { sitSaid = "Kick Fit can't hear the rest of the song yet: send everything except the vocal to this Kick Fit's sidechain, play, then press Sit it."; return; }
            if (! proc.engine.vocalGain (t.vocLo, t.vocMid, t.vocHi, change)) { sitSaid = "Play a part with singing for a few seconds first, then press Sit it."; return; }
            if (std::abs (change) < 0.05) { sitSaid = "The vocal already sits where your " + juce::String (t.name) + " songs have it. Left alone."; return; }
            const float before = proc.get ("level"), after = juce::jlimit (-24.0f, 24.0f, before + (float) change);
            proc.setParam ("level", after);
            sitSaid = juce::String (after > before ? "Turned the vocal up " : "Turned the vocal down ") + juce::String (std::abs (after - before), 1) + " dB to where your " + t.name + " songs have it. Your ears do the rest.";
            return;
        }
        if (! t.measured) { sitSaid = juce::String ("Not enough of your ") + t.name + " songs were measured to know where the kick should sit. Pick another genre or set Level by ear."; return; }
        const juce::ScopedLock sl (linkLock);
        if (! ctx().hasSidechain()) { sitSaid = "Kick Fit can't hear the bass yet. Put a second Kick Fit on the bass, set it to The bass, and send the kick to its sidechain. Then play and press Sit it."; return; }
        if (! ctx().sitGain (t.sitLo, t.sitMid, t.sitHi, change) || ! ctx().sitLevel (now)) { sitSaid = "Play a few bars with the kick first, then press Sit it."; return; }
        if (std::abs (change) < 0.05) { sitSaid = "The kick already sits where your " + juce::String (t.name) + " songs have it. Left alone."; return; }
        const float before = proc.get ("level"), after = juce::jlimit (-24.0f, 24.0f, before + (float) change), moved = after - before;
        proc.setParam ("level", after);
        const bool short_ = std::abs (moved - (float) change) > 0.3f;          // Level ran out before the kick got there
        sitSaid = juce::String (moved > 0 ? "Turned the kick up " : "Turned the kick down ") + juce::String (std::abs (moved), 1) + " dB"
                + (short_ ? juce::String (". That is as far as Level goes and it is still ") + (change > 0 ? "quieter" : "louder") + " than your " + t.name + " songs: move the kick's fader too."
                          : juce::String (" to where your ") + t.name + " songs have it. Your ears do the rest.");
    }

    void timerCallback() override
    {
        std::vector<float> buf ((size_t) fftSize * 2);
        auto spectrum = [&] (const Scope& s, std::vector<float>& mag)
        {
            std::fill (buf.begin(), buf.end(), 0.0f); s.latest (buf.data(), fftSize); window.multiplyWithWindowingTable (buf.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (buf.data());
            mag.resize ((size_t) fftSize / 2, 0.0f);
            for (size_t i = 0; i < mag.size(); ++i) { const float v = buf[i] / (fftSize * 0.25f); mag[i] = v > mag[i] ? v : mag[i] * 0.86f + v * 0.14f; }
        };
        const int role = (int) proc.get ("role"); const bool onBass = role == 1, onVocal = role >= 2, onRest = role == 3;
        {
            const juce::ScopedLock sl (linkLock);
            auto* b = role != 0 ? nullptr : bassOne.load();
            paired = b != nullptr && ! proc.engine.hasSidechain() && b->engine.hasSidechain();
            spectrum (proc.mine, magMine); spectrum (paired ? b->mine : proc.other, magOther);
        }
        const bool led = onBass && linkedRoom.load() >= 0;                     // a Kick Fit on the kick sets Room for this one
        for (int i = 0; i < knobs.size(); ++i) { const bool on = onVocal ? i == 6 : onBass ? ((i == 4 && ! led) || i == 6) : true; knobs[i]->setEnabled (on); knobs[i]->setAlpha (on ? 1.0f : 0.3f); }
        sitBtn.setEnabled (! onBass && ! onRest);
        keys.add ((onBass ? magMine : magOther).data(), fftSize / 2, proc.rate / fftSize);
        const double hz = onVocal ? 0.0 : proc.engine.noteShown(), tune = proc.get ("tune");
        shownHz = hz > 0 ? hz * kf::Engine::ratioOf (tune) : 0;
        int tonic = 0; bool minor = false; double sure = 0; juce::String keyText; bool haveKey = false;
        const int pick = (int) proc.get ("key");
        if (pick > 0) { tonic = (pick - 1) % 12; minor = pick > 12; haveKey = true; keyText = juce::String (noteNames[tonic]) + (minor ? " minor" : " major"); }
        else if (keys.key (tonic, minor, sure) && sure > 0.04) { haveKey = true; keyText = juce::String (noteNames[tonic]) + (minor ? " minor" : " major") + " (heard on the sidechain)"; }
        canTune = false;
        juce::String line;
        if (onRest) line = "On everything else: nothing changes here unless you move Level. This Kick Fit tells Fit Bus what the rest of the song is doing.";
        else if (onVocal) line = ! proc.engine.hasSidechain() ? "On the vocal: send everything except the vocal to this Kick Fit's sidechain, so it can hear the rest of the song."
                                                          : "On the vocal: the bar shows how loud the singing is against everything else. Sit it sets Level.";
        else if (onBass) line = ! proc.engine.hasSidechain() ? "On the bass: send the kick to this Kick Fit's sidechain so it knows when the kick hits."
                         : led ? "On the bass: its lows dip when the kick hits. Room for bass is set on the kick's Kick Fit."
                               : "On the bass: its lows dip when the kick on the sidechain hits. Room for bass sets how far.";
        else if (hz <= 0) line = "Play the kick: Kick Fit reads its boom note.";
        else
        {
            line = "Boom: " + noteText (shownHz) + (proc.engine.glideSemis() > 0.5 && proc.engine.glideSemis() < 36 ? " (drops " + juce::String (proc.engine.glideSemis(), 1) + " semitones at the start)" : juce::String()) + ". ";
            const double apart = proc.engine.noteSpreadCents(); const bool moving = apart > 30;
            if (moving) line += "It reads a little differently on every hit (" + juce::String ((int) apart) + " cents apart), so it has no one clear note: tune by ear. ";
            if (haveKey && ! moving)
            {
                int pc = 0; const double move = kf::tuneToKey (shownHz, tonic, minor, &pc);
                if (std::abs (move) < 0.15) line += "That fits " + keyText + ".";
                else { tuneMove = tune + move; canTune = std::abs (tuneMove) <= 12; line += "Key: " + keyText + ". " + juce::String (noteNames[pc]) + " is the nearest note that fits."; tuneBtn.setButtonText (juce::String ("Tune to ") + noteNames[pc] + " (" + (move > 0 ? "+" : "") + juce::String (move, 2) + ")"); }
            }
            else if (! haveKey) line += "Key not known yet: pick it at the top, or put a Kick Fit on the bass so it can listen.";
        }
        if (! canTune) tuneBtn.setButtonText (std::abs (tune) > 0.005 ? "Tuned" : "Tune to key");
        tuneBtn.setEnabled (canTune);
        said.setText (sitSaid.isNotEmpty() && ++sitShown < 24 * 8 ? sitSaid : line, juce::dontSendNotification);
        if (sitShown >= 24 * 8) { sitSaid.clear(); sitShown = 0; }
        repaint();
    }

    KickFitProcessor& proc;
    juce::ComboBox role, mode, key; juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boxes;
    juce::OwnedArray<juce::Slider> knobs; juce::OwnedArray<juce::Label> labels; juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliders;
    juce::TextButton tuneBtn, sitBtn; juce::Label said; juce::Rectangle<int> picture;
    juce::dsp::FFT fft; juce::dsp::WindowingFunction<float> window; std::vector<float> magMine, magOther;
    kf::Engine& ctx() { auto* b = bassOne.load(); return paired && b != nullptr ? b->engine : proc.engine; }     // call with linkLock held
    kf::KeyFinder keys; double shownHz = 0, tuneMove = 0; bool canTune = false, paired = false; juce::String sitSaid; int sitShown = 0;
};

juce::AudioProcessorEditor* KickFitProcessor::createEditor() { return new KickFitEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KickFitProcessor(); }
