// Fit Bus: the plug-in and its window. The sound work is in FitBus.h.
// A limiter for the bus the kick, bass and vocal meet on. Its picture is the level over time, coloured by pitch:
// red at the very bottom, orange for bass, through to deep blue at the top, on a plain dB scale.
// Each Kick Fit in the song tells it what it is on (FitLink.h), so it knows the kick, bass and vocal for certain, works out
// how tall each stands in the bus after the faders, and compares that with the measured songs of the chosen genre.
#include <juce_audio_utils/juce_audio_utils.h>
#include "FitBus.h"
#include "../kickfit/KickFitTargets.h"

class FitBusProcessor : public juce::AudioProcessor
{
public:
    FitBusProcessor()
        : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          state (*this, nullptr, "FitBus", layout())
    {
        gain = state.getRawParameterValue ("gain"); ceiling = state.getRawParameterValue ("ceiling");
        for (int b = 0; b < fb::numBands; ++b) toneRaw[b] = state.getRawParameterValue (juce::String ("tone") + juce::String (b + 1));
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout()
    {
        using F = juce::AudioParameterFloat; juce::AudioProcessorValueTreeState::ParameterLayout l;
        juce::StringArray modes; for (auto& m : kf::modeTargets) modes.add (m.name);
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Genre", modes, 1));
        l.add (std::make_unique<F> (juce::ParameterID { "gain", 1 }, "Loudness", juce::NormalisableRange<float> (0, 24, 0.1f), 0.0f));
        l.add (std::make_unique<F> (juce::ParameterID { "ceiling", 1 }, "Ceiling", juce::NormalisableRange<float> (-12, 0, 0.1f), 0.0f));
        l.add (std::make_unique<F> (juce::ParameterID { "attack", 1 }, "Attack", juce::NormalisableRange<float> (-100, 100, 1), 0.0f));
        l.add (std::make_unique<F> (juce::ParameterID { "sustain", 1 }, "Sustain", juce::NormalisableRange<float> (-100, 100, 1), 0.0f));
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "tame", 1 }, "Tame a range", juce::StringArray { "Off", "Sub (under 60 Hz)", "Bass (60-150 Hz)", "Low mids (150-400 Hz)", "Mids (400-1500 Hz)", "Presence (1.5-5 kHz)", "Air (over 5 kHz)" }, 0));
        l.add (std::make_unique<F> (juce::ParameterID { "tameThr", 1 }, "Tame threshold", juce::NormalisableRange<float> (-60, 0, 0.5f), -24.0f));
        l.add (std::make_unique<F> (juce::ParameterID { "tameAmt", 1 }, "Tame amount", juce::NormalisableRange<float> (0, 100, 1), 50.0f));
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "tameHear", 1 }, "Hear the range", false));
        for (int b = 0; b < fb::numBands; ++b)
            l.add (std::make_unique<F> (juce::ParameterID { juce::String ("tone") + juce::String (b + 1), 1 }, juce::String ("Tone ") + juce::String (b + 1), juce::NormalisableRange<float> (-6, 6, 0.1f), 0.0f));
        l.add (std::make_unique<F> (juce::ParameterID { "toneAmt", 1 }, "Tone amount", juce::NormalisableRange<float> (0, 100, 1), 100.0f));
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "toneOn", 1 }, "Tone change on", true));
        return l;
    }
    const juce::String getName() const override { return "Fit Bus"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto in = l.getMainInputChannelSet(); return in == l.getMainOutputChannelSet() && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
    }
    void prepareToPlay (double sr, int) override { engine.prepare (sr); rate = sr; setLatencySamples (engine.latency()); }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        const int n = buffer.getNumSamples(); if (n == 0 || buffer.getNumChannels() == 0) return;
        fb::Params p; p.gainDb = gain->load(); p.ceilingDb = ceiling->load();
        p.attack = raw ("attack") / 100; p.sustain = raw ("sustain") / 100;
        p.tameRange = (int) raw ("tame"); p.tameThrDb = raw ("tameThr"); p.tameAmount = raw ("tameAmt") / 100; p.tameHear = raw ("tameHear") > 0.5f;
        for (int b = 0; b < fb::numBands; ++b) p.tone[b] = toneRaw[b]->load() * raw ("toneAmt") / 100;
        p.toneOn = raw ("toneOn") > 0.5f;
        engine.set (p);
        float* l = buffer.getWritePointer (0); float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        if (r != nullptr) { engine.process (l, r, n); return; }
        spare.resize ((size_t) n); std::copy (l, l + n, spare.begin()); engine.process (l, spare.data(), n);       // mono: the same sound on both sides
    }
    void getStateInformation (juce::MemoryBlock& dest) override { if (auto xml = state.copyState().createXml()) copyXmlToBinary (*xml, dest); }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size)) if (xml->hasTagName (state.state.getType())) state.replaceState (juce::ValueTree::fromXml (*xml));
    }

    float raw (const char* id) const { return state.getRawParameterValue (id)->load(); }
    void put (const juce::String& id, float value) { if (auto* q = state.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (value)); q->endChangeGesture(); } }
    std::atomic<float>* toneRaw[fb::numBands] {};
    juce::AudioProcessorValueTreeState state; fb::Engine engine; double rate = 44100;
private:
    std::atomic<float>* gain = nullptr; std::atomic<float>* ceiling = nullptr; std::vector<float> spare;
};

namespace look
{
const juce::Colour paper (0xff121314), panel (0xff1b1d1f), line (0xff2c2f33), ink (0xffe9e6df), soft (0xff9a968d);
// lowest pitch first: red, orange, yellow, green, light blue, deep blue
const juce::Colour bands[fb::numBands] = { juce::Colour (0xffd9342b), juce::Colour (0xfff08a24), juce::Colour (0xffe6c93a), juce::Colour (0xff4fbf6b), juce::Colour (0xff3d9be0), juce::Colour (0xff2438b8) };
const char* const bandNames[fb::numBands] = { "Sub (under 60 Hz)", "Bass", "Low mids", "Mids", "Presence", "Air (over 5 kHz)" };
}

class FitBusEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FitBusEditor (FitBusProcessor& p) : AudioProcessorEditor (p), proc (p)
    {
        auto knob = [&] (juce::Slider& s, juce::Label& l, const char* id, const char* name, const char* unit)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18); s.setTextValueSuffix (unit);
            s.setColour (juce::Slider::rotarySliderFillColourId, look::bands[1]); s.setColour (juce::Slider::textBoxTextColourId, look::ink); s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            l.setText (name, juce::dontSendNotification); l.setJustificationType (juce::Justification::centred); l.setColour (juce::Label::textColourId, look::soft); l.setFont (juce::FontOptions (12.0f));
            addAndMakeVisible (s); addAndMakeVisible (l); links.add (new juce::AudioProcessorValueTreeState::SliderAttachment (proc.state, id, s));
        };
        knob (gain, gainL, "gain", "Loudness", " dB"); knob (ceiling, ceilL, "ceiling", "Ceiling", " dB");
        knob (attack, attackL, "attack", "Attack", " %"); knob (sustain, sustainL, "sustain", "Sustain", " %");
        knob (tameThr, tameThrL, "tameThr", "Threshold", " dB"); knob (tameAmt, tameAmtL, "tameAmt", "Amount", " %");
        addAndMakeVisible (tame); tame.setColour (juce::ComboBox::backgroundColourId, look::panel); tame.setColour (juce::ComboBox::textColourId, look::ink); tame.setColour (juce::ComboBox::outlineColourId, look::line);
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.state.getParameter ("tame"))) tame.addItemList (ch->choices, 1);
        tameLink = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.state, "tame", tame);
        addAndMakeVisible (hear); hear.setButtonText ("Hear it"); hear.setClickingTogglesState (true);
        hear.setColour (juce::TextButton::buttonColourId, look::panel); hear.setColour (juce::TextButton::buttonOnColourId, look::bands[1]); hear.setColour (juce::TextButton::textColourOffId, look::ink);
        hearLink = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state, "tameHear", hear);
        addAndMakeVisible (mode); mode.setColour (juce::ComboBox::backgroundColourId, look::panel); mode.setColour (juce::ComboBox::textColourId, look::ink); mode.setColour (juce::ComboBox::outlineColourId, look::line);
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.state.getParameter ("mode"))) mode.addItemList (ch->choices, 1);
        modeLink = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.state, "mode", mode);
        for (int k = 0; k < 3; ++k)
        {
            addAndMakeVisible (fix[k]); fix[k].setButtonText ("Fix"); fix[k].setEnabled (false);
            fix[k].setColour (juce::TextButton::buttonColourId, look::panel); fix[k].setColour (juce::TextButton::textColourOffId, look::ink);
            fix[k].onClick = [this, k] { doFix (k); };
        }
        addAndMakeVisible (setBtn); setBtn.setButtonText ("Set it"); setBtn.setColour (juce::TextButton::buttonColourId, look::panel); setBtn.setColour (juce::TextButton::textColourOffId, look::ink);
        setBtn.onClick = [this] { setLoudness(); };
        for (auto* bt : { &balanceBtn, &resetBtn, &abBtn }) { addAndMakeVisible (*bt); bt->setColour (juce::TextButton::buttonColourId, look::panel); bt->setColour (juce::TextButton::textColourOffId, look::ink); bt->setColour (juce::TextButton::textColourOnId, look::ink); }
        balanceBtn.setButtonText ("Balance it"); balanceBtn.onClick = [this] { balance(); };
        resetBtn.setButtonText ("Reset"); resetBtn.onClick = [this] { for (int b = 0; b < fb::numBands; ++b) proc.put (juce::String ("tone") + juce::String (b + 1), 0.0f); toneSaid = "Tone change cleared."; toneCool = 150; };
        abBtn.setClickingTogglesState (true); abBtn.setColour (juce::TextButton::buttonOnColourId, look::bands[3].darker (0.6f));
        abLink = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state, "toneOn", abBtn);
        addAndMakeVisible (toneAmt); toneAmt.setSliderStyle (juce::Slider::LinearHorizontal); toneAmt.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); toneAmt.setColour (juce::Slider::trackColourId, look::bands[1]);
        toneAmtLink = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.state, "toneAmt", toneAmt);
        startTimerHz (30); setSize (860, 664 + stripHeight);                      // last: resized() needs the controls
    }
    void resized() override
    {
        auto side = getLocalBounds().reduced (14).removeFromRight (120); side.removeFromTop (40);
        gainL.setBounds (side.removeFromTop (16)); gain.setBounds (side.removeFromTop (100)); side.removeFromTop (14);
        ceilL.setBounds (side.removeFromTop (16)); ceiling.setBounds (side.removeFromTop (100));
        { auto lb = getLocalBounds().reduced (14).removeFromRight (120); lb.removeFromTop (loudTop + 16 + 22 + 44); setBtn.setBounds (lb.removeFromTop (24)); }
        {
            auto tc = getLocalBounds().reduced (14).removeFromRight (120); tc.removeFromTop (toneCtl);
            auto row = tc.removeFromTop (24); balanceBtn.setBounds (row.removeFromLeft (70)); row.removeFromLeft (4); resetBtn.setBounds (row);
            tc.removeFromTop (4); abBtn.setBounds (tc.removeFromTop (24)); tc.removeFromTop (2); toneAmt.setBounds (tc.removeFromTop (20).withTrimmedLeft (46));
        }
        auto area = getLocalBounds().reduced (14); area.removeFromRight (130);
        {
            auto strip = area.removeFromBottom (stripHeight).withTrimmedTop (30);
            auto place = [&] (juce::Slider& sl, juce::Label& lb) { auto c = strip.removeFromLeft (84); lb.setBounds (c.removeFromTop (14)); sl.setBounds (c); };
            place (attack, attackL); place (sustain, sustainL); strip.removeFromLeft (40);
            auto pick = strip.removeFromLeft (190); tame.setBounds (pick.removeFromTop (26)); pick.removeFromTop (8); hear.setBounds (pick.removeFromTop (24).removeFromLeft (80));
            strip.removeFromLeft (12); place (tameThr, tameThrL); place (tameAmt, tameAmtL);
        }
        mode.setBounds (area.removeFromTop (30).withTrimmedLeft (90).removeFromLeft (200).reduced (0, 3));
        area.removeFromBottom (22); auto rows = area.removeFromBottom (rowsHeight);
        for (int k = 0; k < 3; ++k) fix[k].setBounds (rows.removeFromTop (26).removeFromRight (84).reduced (0, 2));
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (look::paper);
        auto area = getLocalBounds().reduced (14); area.removeFromRight (130);
        {
            auto strip = area.removeFromBottom (stripHeight); g.setColour (look::line); g.fillRect (strip.getX(), strip.getY() + 6, strip.getWidth(), 1);
            auto head = strip.withTrimmedTop (10).removeFromTop (18); g.setColour (look::ink); g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText ("Transients", head.removeFromLeft (168), juce::Justification::centredLeft); head.removeFromLeft (40);
            g.drawText ("Tame a range (holds it down only when it jumps out)", head, juce::Justification::centredLeft);
            const bool on = proc.raw ("tame") > 0.5f; tameShown = std::min (proc.engine.tameDb(), tameShown * 0.97f); const float held = tameShown;          // the deepest of the last second or so, so the words don't flicker
            g.setColour (look::soft); g.setFont (juce::FontOptions (11.0f));
            g.drawFittedText (! on ? "Pick a range to watch." : proc.raw ("tameHear") > 0.5f ? "You are hearing only the range it watches. Switch Hear it off when done."
                              : held < -0.1f ? juce::String ("Holding it down by up to ") + juce::String (-held, 1) + " dB." : "Under the threshold: nothing is being changed.",
                              strip.withTrimmedLeft (168 + 40 + 190 + 12 + 168 + 10).withTrimmedTop (40).reduced (0, 6), juce::Justification::topLeft, 4);
        }
        g.setColour (look::ink); g.setFont (juce::FontOptions (18.0f, juce::Font::bold)); g.drawText ("Fit Bus", area.removeFromTop (30), juce::Justification::centredLeft);
        auto legend = area.removeFromBottom (22); auto rows = area.removeFromBottom (rowsHeight);
        g.setFont (juce::FontOptions (13.0f));
        for (int k = 0; k < 3; ++k) { auto row = rows.removeFromTop (26); row.removeFromRight (90); g.setColour (part[k].warn ? look::bands[1] : look::ink); g.drawText (part[k].text, row, juce::Justification::centredLeft); }
        g.setColour (look::soft); g.setFont (juce::FontOptions (12.0f)); g.drawFittedText (note, rows.removeFromTop (34), juce::Justification::centredLeft, 2);
        auto said = area.removeFromBottom (26);
        g.setColour (look::panel); g.fillRoundedRectangle (area.toFloat(), 4.0f); g.setColour (look::line); g.drawRoundedRectangle (area.toFloat(), 4.0f, 1.0f);
        auto pic = area.reduced (1).withTrimmedLeft (34);
        const float top = 6.0f, bottom = -42.0f;                     // dB shown, a plain scale: every 6 dB is the same distance
        auto y = [&] (float dB) { return (float) pic.getY() + (float) pic.getHeight() * (top - juce::jlimit (bottom, top, dB)) / (top - bottom); };
        auto dB = [] (float v) { return 20.0f * std::log10 (std::max (v, 1e-6f)); };
        g.setFont (juce::FontOptions (10.0f));
        for (int d = 0; d >= -36; d -= 6) { g.setColour (look::line); g.fillRect ((float) pic.getX(), y ((float) d), (float) pic.getWidth(), 1.0f); g.setColour (look::soft); g.drawText (juce::String (d), area.getX() + 2, (int) y ((float) d) - 6, 28, 12, juce::Justification::centredRight); }

        const int n = std::min (fb::Engine::numColumns - 2, pic.getWidth() / 2); std::vector<fb::Column> c ((size_t) n); proc.engine.latest (c.data(), n);
        const float w = (float) pic.getWidth() / (float) n, floorY = (float) pic.getBottom(); float loudest = 0, held = 1;
        for (int i = 0; i < n; ++i)
        {
            const auto& k = c[(size_t) i]; const float x = (float) pic.getX() + w * (float) i; loudest = std::max (loudest, k.out); held = std::min (held, k.gain);
            if (k.out <= 1e-5f) continue;
            const float yo = y (dB (k.out)), h = floorY - yo; float sum = 0; for (float b : k.band) sum += b;
            if (sum <= 0 || h <= 0) continue;
            float at = floorY;                                       // stacked: the lowest pitch at the bottom, each range as tall as its share
            for (int b = 0; b < fb::numBands; ++b) { const float part = h * k.band[b] / sum; g.setColour (look::bands[b]); g.fillRect (x, at - part, w + 0.5f, part); at -= part; }
            if (k.gain < 0.999f) { const float yi = y (dB (k.in)); g.setColour (look::ink.withAlpha (0.28f)); g.fillRect (x, yi, w + 0.5f, std::max (0.0f, yo - yi)); }      // what the limiter took off
        }
        const float cy = y ((float) ceiling.getValue()); g.setColour (look::ink); g.fillRect ((float) pic.getX(), cy, (float) pic.getWidth(), 1.5f);
        g.setFont (juce::FontOptions (10.0f)); g.drawText ("ceiling", pic.getRight() - 60, (int) cy - 13, 56, 12, juce::Justification::centredRight);

        g.setFont (juce::FontOptions (13.0f)); g.setColour (look::ink);
        juce::String line = loudest <= 1e-5f ? "Play the song: the picture shows how tall it is, coloured by pitch."
                          : "Loudest peak in view: " + juce::String (dB (loudest), 1) + " dB. " + (held < 0.999f ? "The limiter held it down by up to " + juce::String (-dB (held), 1) + " dB (the pale tops)." : "The limiter has not had to do anything.");
        g.drawText (line, said, juce::Justification::centredLeft);
        {   // tone balance: how the sound is shared between the six ranges, against the finished songs of this genre
            const auto& t = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.state.getRawParameterValue ("mode")->load())];
            auto box = getLocalBounds().reduced (14).removeFromRight (120); box.removeFromTop (300);
            g.setColour (look::ink); g.setFont (juce::FontOptions (12.0f, juce::Font::bold)); g.drawText ("Tone", box.removeFromTop (16), juce::Justification::centredLeft);
            g.setColour (look::soft); g.setFont (juce::FontOptions (10.0f));
            g.drawFittedText (t.tone ? juce::String ("Bars: what leaves here. Shaded: your ") + t.name + " songs." : juce::String ("Not enough ") + t.name + " songs measured for a range.", box.removeFromTop (26), juce::Justification::topLeft, 2);
            auto xOf = [&] (float dB, juce::Rectangle<int> tr) { return (float) tr.getX() + (float) tr.getWidth() * juce::jlimit (0.0f, 1.0f, (dB + 36.0f) / 36.0f); };
            for (int b = fl::numBands - 1; b >= 0; --b)                 // highest pitch at the top, like the picture
            {
                auto tr = box.removeFromTop (30).reduced (0, 9);
                g.setColour (look::panel); g.fillRect (tr);
                if (t.tone) { g.setColour (look::ink.withAlpha (0.22f)); g.fillRect (xOf (t.toneDb[b][0], tr), (float) tr.getY(), xOf (t.toneDb[b][2], tr) - xOf (t.toneDb[b][0], tr), (float) tr.getHeight()); }
                if (toneSeen)
                {
                    const bool out = t.tone && (toneNow[b] > t.toneDb[b][2] + 2.0f || toneNow[b] < t.toneDb[b][0] - 2.0f);
                    g.setColour (look::bands[b]); g.fillRect ((float) tr.getX(), (float) tr.getY() + 3, xOf (toneNow[b], tr) - (float) tr.getX(), (float) tr.getHeight() - 6);
                    if (out) { g.setColour (look::ink); g.drawRect (tr.toFloat(), 1.0f); }          // more than 2 dB outside the range: outlined
                }
                const float moved = proc.toneRaw[b]->load() * proc.raw ("toneAmt") / 100;              // the tone change on this range, when there is one
                if (std::abs (moved) >= 0.05f) { g.setColour (look::ink); g.setFont (juce::FontOptions (10.0f, juce::Font::bold)); g.drawText ((moved > 0 ? "+" : "") + juce::String (moved, 1), tr.translated (0, -11).withHeight (11), juce::Justification::centredRight); }
            }
            g.setColour (look::soft); g.setFont (juce::FontOptions (10.0f));
            g.drawFittedText (toneSaid.isNotEmpty() && toneCool > 0 ? toneSaid : juce::String ("Outlined = more than 2 dB outside your songs."), box.removeFromTop (34), juce::Justification::topLeft, 3);
            box.removeFromTop (24 + 4 + 24 + 2); g.drawText ("Amount", box.removeFromTop (20).removeFromLeft (46), juce::Justification::centredLeft);
        }
        {   // loudness of what leaves, against the finished songs of this genre
            const auto& t = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.state.getRawParameterValue ("mode")->load())];
            auto box = getLocalBounds().reduced (14).removeFromRight (120); box.removeFromTop (loudTop);
            g.setColour (look::ink); g.setFont (juce::FontOptions (12.0f, juce::Font::bold)); g.drawText ("How loud", box.removeFromTop (16), juce::Justification::centredLeft);
            auto tr = box.removeFromTop (22).reduced (0, 6); auto xOf = [&] (float lufs) { return (float) tr.getX() + (float) tr.getWidth() * juce::jlimit (0.0f, 1.0f, (lufs + 26.0f) / 22.0f); };          // -26 to -4 LUFS
            g.setColour (look::panel); g.fillRect (tr);
            if (t.loud) { g.setColour (look::ink.withAlpha (0.22f)); g.fillRect (xOf (t.lufs[0]), (float) tr.getY(), xOf (t.lufs[2]) - xOf (t.lufs[0]), (float) tr.getHeight()); }
            if (loudOk) { g.setColour (look::bands[1]); g.fillRect (xOf ((float) loudNow) - 2, (float) tr.getY() - 3, 4.0f, (float) tr.getHeight() + 6); }
            g.setColour (look::soft); g.setFont (juce::FontOptions (10.0f));
            g.drawFittedText (loudSaid.isNotEmpty() && loudCool > 0 ? loudSaid
                              : loudOk ? juce::String (loudNow, 1) + " LUFS since play began." + (t.loud ? juce::String (" Shaded: your ") + t.name + " songs." : juce::String (" Not enough ") + t.name + " songs measured.")
                                       : juce::String ("Play a few seconds for a reading."), box.removeFromTop (44), juce::Justification::topLeft, 4);
        }
        g.setFont (juce::FontOptions (11.0f)); int lx = legend.getX();
        for (int b = 0; b < fb::numBands; ++b)
        {
            g.setColour (look::bands[b]); g.fillRect (lx, legend.getY() + 6, 12, 12); g.setColour (look::soft);
            const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), look::bandNames[b]) + 8; g.drawText (look::bandNames[b], lx + 16, legend.getY() + 2, tw, 20, juce::Justification::centredLeft); lx += tw + 26;
        }
    }
private:
    struct Part { juce::String text; bool warn = false; float fixDb = 0; int slot = -1; };
    static constexpr int rowsHeight = 26 * 3 + 34, stripHeight = 130;

    // 30 times a second: read what the Kick Fits say, work out how much of each is in the bus, compare with the measured songs
    void timerCallback() override
    {
        static const char* const names[3] = { "Kick", "Bass", "Vocal" }; static const char* const on[3] = { "The kick", "The bass", "The vocal" };
        const auto& t = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.state.getRawParameterValue ("mode")->load())];
        auto* b = fl::shared(); bool have[4] = { false, false, false, false }; double P[4][fl::numBands] {}, peak[4] {}, usual[4] {}, B[fl::numBands];
        for (auto& q : part) q.slot = -1;
        if (b != nullptr)
            for (int i = 0; i < fl::maxSlots; ++i)
            {
                const auto& sl = b->slot[i]; const int r = sl.role.load();
                if (! fl::alive (sl) || r < 0 || r > 3 || have[r]) continue;                 // the first of each kind counts
                have[r] = true; if (r < 3) part[r].slot = i;
                peak[r] = sl.peak.load(); usual[r] = sl.usual.load(); for (int k = 0; k < fl::numBands; ++k) P[r][k] = sl.power[k].load();
            }
        if (have[0] != had[0] || have[1] != had[1] || have[2] != had[2] || have[3] != had[3]) { mixer.reset(); for (int k = 0; k < 4; ++k) had[k] = have[k]; }
        for (int k = 0; k < fl::numBands; ++k) B[k] = proc.engine.inPower[k].load();
        const double song = proc.engine.inPeak.load();
        if (proc.engine.inNow.load() > 1e-4f) quiet = 0; else if (quiet < 1000) ++quiet;
        const bool playing = quiet < 15;                                   // sound arrived in the last half second
        if (playing != wasPlaying) { wasPlaying = playing; if (playing) { mixer.reset(); proc.engine.loud.clear(); loudOk = false; } }          // start afresh each time the song starts
        if (cool > 0) --cool;
        if (loudCool > 0) --loudCool;
        if (toneCool > 0) --toneCool;
        abBtn.setButtonText (abBtn.getToggleState() ? "Tone change: on" : "Off (same loudness)");
        if (playing) { double v = 0; if (proc.engine.loud.read (v)) { loudNow = v; loudOk = true; } }
        if (! playing)                                                     // stopped: keep the last reading on screen
        {
            note = b == nullptr ? "This computer would not let Fit Bus and Kick Fit talk to each other, so Fit Bus only limits and draws."
                 : ! (have[0] || have[1] || have[2]) ? "No Kick Fit found. Put Kick Fit on the kick, the bass and the vocal, set each one's \"I am on\", and send those tracks here."
                 : read ? "Stopped. Showing the last reading." : "Play the song.";
            if (! read) for (int k = 0; k < 3; ++k) part[k].text = juce::String (names[k]) + (have[k] ? ": found." : juce::String (": no Kick Fit set to \"") + on[k] + "\" is running.");
            for (auto& f : fix) f.setEnabled (false);
            repaint(); return;
        }
        mixer.add (P, have, B, 1.0 / 30.0);
        { double all = 0; for (int k = 0; k < fl::numBands; ++k) { toneP[k] += (toneSeen ? toneEase : 1.0) * ((double) proc.engine.outPower[k].load() - toneP[k]); all += toneP[k]; }          // about 8 s
          toneSeen = all > 1e-10; if (toneSeen) for (int k = 0; k < fl::numBands; ++k) toneNow[k] = (float) (10 * std::log10 (std::max (toneP[k] / all, 1e-6))); }
        if (logOn)                       // for working on Fit Bus: the numbers it sees, 30 lines a second (only when the switch file exists)
        {
            juce::String row; for (int k = 0; k < 3; ++k) { row << (have[k] ? 1 : 0) << "," << peak[k] << ","; for (int i = 0; i < fl::numBands; ++i) row << P[k][i] << ","; }
            for (int i = 0; i < fl::numBands; ++i) row << B[i] << ",";
            row << song << "\n"; logFile.appendText (row);
        }
        double g2[4], fit = 0; const bool solved = mixer.solve (have, g2, fit), sure = solved && fit >= 0.97;          // measured: 0.999 on a real three-part project, 0.79 to 0.94 with unlabelled music in the bus
        read = read || sure;
        note = ! solved ? "Listening..."
             : ! sure ? "There is more in this bus than the parts with a Kick Fit on them, so their heights can't be read. Put a Kick Fit set to Everything else on the rest of the music, or send only the kick, bass and vocal here."
             : said.isNotEmpty() && cool > 0 ? said
             : have[3] ? "Measured after the faders, with the rest of the song counted in (its Kick Fit is set to Everything else). The gaps come from your finished songs."
             : "Measured after the faders. The gaps come from finished songs, where other instruments share the low end with the bass: with only these parts playing, trust your ears on the bass.";
        auto rel = [] (float v) { return juce::String (std::abs (v), 1) + (v < 0 ? " dB under" : " dB over"); };
        const double kickTop = have[0] && sure && g2[0] > 1e-9 ? peak[0] * std::sqrt (g2[0]) : 0.0;          // the kick's hits as they arrive here
        for (int k = 0; k < 3; ++k)
        {
            auto& q = part[k]; q.warn = false; q.fixDb = 0;
            double mine = 0, all = 0; for (int i = 0; i < fl::numBands; ++i) { mine += g2[k] * P[k][i]; all += B[i]; }
            if (! have[k]) q.text = juce::String (names[k]) + ": no Kick Fit set to \"" + on[k] + "\" is running.";
            else if (! sure) q.text = juce::String (names[k]) + ": found.";
            else if (g2[k] <= 1e-9 || peak[k] <= 1e-6) q.text = juce::String (names[k]) + ": found, but none of it reaches this bus.";
            else if (k == 0) q.text = "Kick: its hits peak " + (kickTop < song * 0.994 ? rel ((float) (20 * std::log10 (kickTop / song))) : juce::String ("at")) + " the loudest peak here. The bass and vocal are compared with it.";
            else if (kickTop <= 0) q.text = juce::String (names[k]) + ": found. It is compared with the kick, and no kick reaches this bus.";
            else if (usual[k] <= 1e-6) q.text = juce::String (names[k]) + ": found. Waiting for more of it to play.";
            else if (mine < 0.03 * all) q.text = juce::String (names[k]) + ": too quiet next to the others here to read exactly.";          // under about 15 dB down the reading can be several dB out
            else
            {
                const float* range = k == 1 ? t.bassVsKick : t.vocVsKick;
                const float h = (float) (20 * std::log10 (usual[k] * std::sqrt (g2[k]) / kickTop));          // its usual peak against the kick's hits
                q.text = juce::String (names[k]) + ": usually peaks " + rel (h) + " the kick. ";
                if (! t.peaks) q.text << "Not enough " << t.name << " songs measured to compare.";
                else
                {
                    q.text << "Your " << t.name << " songs: " << rel (range[0]) << " to " << rel (range[2]) << ". ";
                    if (h > range[2] + 0.5f) { q.warn = true; q.fixDb = range[1] - h; q.text << "Taller than most."; }
                    else if (h < range[0] - 0.5f) { q.warn = true; q.fixDb = range[1] - h; q.text << "Lower than most."; }
                    else q.text << "In range.";
                }
            }
            fix[k].setEnabled (q.fixDb != 0 && cool == 0); fix[k].setButtonText (q.fixDb != 0 ? (q.fixDb > 0 ? "Up " : "Down ") + juce::String (std::abs (q.fixDb), 1) : juce::String ("Fix"));
        }
        repaint();
    }
    // set the six tone gains so each range that sits outside the measured songs moves to their middle; a range already among them is left alone
    void balance()
    {
        static const char* const names[fb::numBands] = { "sub", "bass", "low mids", "mids", "presence", "air" };
        const auto& t = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.state.getRawParameterValue ("mode")->load())];
        toneCool = 360;
        if (! t.tone) { toneSaid = juce::String ("Not enough ") + t.name + " songs measured. Balance by ear."; return; }
        if (! toneSeen || ! wasPlaying) { toneSaid = "Play the song for a few seconds first."; return; }
        if (proc.raw ("toneOn") < 0.5f) proc.put ("toneOn", 1.0f);
        if (proc.raw ("toneAmt") < 99.5f) { proc.put ("toneAmt", 100.0f); toneSaid = "Amount is back at 100%. Let it play a few seconds, then press again."; return; }
        const int tamed = (int) proc.raw ("tame") - 1; int moved = 0; juce::String farOut;
        for (int b = 0; b < fb::numBands; ++b)
        {
            const float now = toneNow[b]; if ((now >= t.toneDb[b][0] && now <= t.toneDb[b][2]) || b == tamed) continue;          // the range Tame is holding is Tame's business
            const float old = proc.toneRaw[b]->load(), want = old + (t.toneDb[b][1] - now), set = juce::jlimit (-6.0f, 4.0f, want);          // a boost past 4 dB mostly lifts hiss
            if (std::abs (want - set) > 1.0f) farOut << (farOut.isEmpty() ? "" : ", ") << names[b];
            if (std::abs (set - old) >= 0.1f) { proc.put (juce::String ("tone") + juce::String (b + 1), set); ++moved; }
        }
        toneSaid = moved == 0 && farOut.isEmpty() ? juce::String ("Already balanced like your ") + t.name + " songs. Left alone."
                 : juce::String (moved) + (moved == 1 ? " range moved." : " ranges moved.") + (farOut.isNotEmpty() ? " Too far out to fix here: " + farOut + ". Change that at its track." : juce::String (" Press again in a few seconds if a bar is still outlined."));
    }
    // turn Loudness so what leaves is as loud as the middle of the measured songs; nothing when it is already among them
    void setLoudness()
    {
        const auto& t = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.state.getRawParameterValue ("mode")->load())];
        double now = 0; loudCool = 300;
        if (! t.loud) { loudSaid = juce::String ("Not enough ") + t.name + " songs measured for a loudness range. Set it by ear."; return; }
        if (! proc.engine.loud.read (now)) { loudSaid = "Play the song for a few seconds first."; return; }
        if (now >= t.lufs[0] && now <= t.lufs[2]) { loudSaid = juce::String ("Already as loud as your ") + t.name + " songs. Left alone."; return; }
        const float ceil = proc.raw ("ceiling"), heldNow = proc.engine.reductionDb();
        // seen in FL: with the Ceiling at -12 dB, 11.8 dB more Loudness made it 1 dB louder. Pushing into a low ceiling only squashes.
        if (now < t.lufs[0] && ceil < -1.0f && heldNow < -1.0f) { loudSaid = juce::String ("The Ceiling is at ") + juce::String (ceil, 1) + " dB and the limiter is already holding the mix under it, so more Loudness would only squash it. Raise the Ceiling toward 0 first."; return; }
        const float before = proc.raw ("gain"), after = juce::jlimit (0.0f, 24.0f, before + (float) (t.lufs[1] - now));
        if (std::abs (after - before) < 0.05f) { loudSaid = now > t.lufs[2] ? "Louder than your songs with Loudness already at 0: turn the parts down." : "Loudness is already at its top."; return; }
        if (auto* p = proc.state.getParameter ("gain")) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (after)); p->endChangeGesture(); }
        proc.engine.loud.clear(); loudOk = false;
        loudSaid = juce::String ("Turned Loudness ") + (after > before ? "up " : "down ") + juce::String (std::abs (after - before), 1) + " dB. The limiter can hold some back: play on, and press again if it still reads short.";
    }
    // ask that part's Kick Fit to change its Level so the part lands in the middle of the measured range
    void doFix (int k)
    {
        static const char* const names[3] = { "kick", "bass", "vocal" };
        auto* b = fl::shared(); const auto q = part[k]; if (b == nullptr || q.slot < 0 || q.fixDb == 0 || cool > 0) return;
        auto& sl = b->slot[q.slot]; const float room = (q.fixDb > 0 ? 24.0f : -24.0f) - sl.level.load();          // what its Level knob has left
        const float ask = q.fixDb > 0 ? std::min (q.fixDb, std::max (0.0f, room)) : std::max (q.fixDb, std::min (0.0f, room));
        if (std::abs (ask) < 0.05f) { said = juce::String ("The Kick Fit on the ") + names[k] + " has its Level at the end of its range. Use the mixer fader."; cool = 150; return; }
        sl.askDb.store (ask); sl.askSeq.fetch_add (1);
        said = juce::String ("Asked the Kick Fit on the ") + names[k] + " to turn " + (ask > 0 ? "up " : "down ") + juce::String (std::abs (ask), 1) + " dB. Give it a few seconds to settle, then look again.";
        cool = 150;                                                 // 5 s: the readings need time to catch up
    }
    FitBusProcessor& proc; juce::Slider gain, ceiling; juce::Label gainL, ceilL; juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> links;
    juce::ComboBox mode; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeLink; juce::TextButton fix[3];
    juce::TextButton balanceBtn, resetBtn, abBtn; juce::Slider toneAmt; juce::String toneSaid; int toneCool = 0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abLink; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAmtLink;
    float tameShown = 0; double loudNow = 0; bool loudOk = false; juce::String loudSaid; int loudCool = 0; juce::TextButton setBtn;
    static constexpr int loudTop = 656, toneCtl = 300 + 16 + 26 + 180 + 34;          // down the right-hand column
    juce::Slider attack, sustain, tameThr, tameAmt; juce::Label attackL, sustainL, tameThrL, tameAmtL; juce::ComboBox tame; juce::TextButton hear;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tameLink; std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hearLink;
    juce::File logFile { juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fitbus-log.csv") };
    bool logOn = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fitbus-log.on").existsAsFile();
    static constexpr double toneEase = 0.011;          // about 3 s: slow enough to be a balance, quick enough to see a knob move it
    double toneP[fl::numBands] {}; float toneNow[fl::numBands] {}; bool toneSeen = false;
    fb::Mixer mixer; Part part[3]; bool had[4] = { false, false, false, false }, wasPlaying = false, read = false; int quiet = 1000; juce::String note, said; int cool = 0;
};

juce::AudioProcessorEditor* FitBusProcessor::createEditor() { return new FitBusEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FitBusProcessor(); }
