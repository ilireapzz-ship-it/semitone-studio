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
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout()
    {
        using F = juce::AudioParameterFloat; juce::AudioProcessorValueTreeState::ParameterLayout l;
        juce::StringArray modes; for (auto& m : kf::modeTargets) modes.add (m.name);
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Genre", modes, 1));
        l.add (std::make_unique<F> (juce::ParameterID { "gain", 1 }, "Loudness", juce::NormalisableRange<float> (0, 24, 0.1f), 0.0f));
        l.add (std::make_unique<F> (juce::ParameterID { "ceiling", 1 }, "Ceiling", juce::NormalisableRange<float> (-12, 0, 0.1f), 0.0f));
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
        fb::Params p; p.gainDb = gain->load(); p.ceilingDb = ceiling->load(); engine.set (p);
        float* l = buffer.getWritePointer (0); float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        if (r != nullptr) { engine.process (l, r, n); return; }
        spare.resize ((size_t) n); std::copy (l, l + n, spare.begin()); engine.process (l, spare.data(), n);       // mono: the same sound on both sides
    }
    void getStateInformation (juce::MemoryBlock& dest) override { if (auto xml = state.copyState().createXml()) copyXmlToBinary (*xml, dest); }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size)) if (xml->hasTagName (state.state.getType())) state.replaceState (juce::ValueTree::fromXml (*xml));
    }

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
        auto knob = [&] (juce::Slider& s, juce::Label& l, const char* id, const char* name)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18); s.setTextValueSuffix (" dB");
            s.setColour (juce::Slider::rotarySliderFillColourId, look::bands[1]); s.setColour (juce::Slider::textBoxTextColourId, look::ink); s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            l.setText (name, juce::dontSendNotification); l.setJustificationType (juce::Justification::centred); l.setColour (juce::Label::textColourId, look::soft); l.setFont (juce::FontOptions (12.0f));
            addAndMakeVisible (s); addAndMakeVisible (l); links.add (new juce::AudioProcessorValueTreeState::SliderAttachment (proc.state, id, s));
        };
        knob (gain, gainL, "gain", "Loudness"); knob (ceiling, ceilL, "ceiling", "Ceiling");
        addAndMakeVisible (mode); mode.setColour (juce::ComboBox::backgroundColourId, look::panel); mode.setColour (juce::ComboBox::textColourId, look::ink); mode.setColour (juce::ComboBox::outlineColourId, look::line);
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.state.getParameter ("mode"))) mode.addItemList (ch->choices, 1);
        modeLink = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.state, "mode", mode);
        for (int k = 0; k < 3; ++k)
        {
            addAndMakeVisible (fix[k]); fix[k].setButtonText ("Fix"); fix[k].setEnabled (false);
            fix[k].setColour (juce::TextButton::buttonColourId, look::panel); fix[k].setColour (juce::TextButton::textColourOffId, look::ink);
            fix[k].onClick = [this, k] { doFix (k); };
        }
        startTimerHz (30); setSize (860, 600);                      // last: resized() needs the controls
    }
    void resized() override
    {
        auto side = getLocalBounds().reduced (14).removeFromRight (120); side.removeFromTop (40);
        gainL.setBounds (side.removeFromTop (16)); gain.setBounds (side.removeFromTop (100)); side.removeFromTop (14);
        ceilL.setBounds (side.removeFromTop (16)); ceiling.setBounds (side.removeFromTop (100));
        auto area = getLocalBounds().reduced (14); area.removeFromRight (130);
        mode.setBounds (area.removeFromTop (30).withTrimmedLeft (90).removeFromLeft (200).reduced (0, 3));
        area.removeFromBottom (22); auto rows = area.removeFromBottom (rowsHeight);
        for (int k = 0; k < 3; ++k) fix[k].setBounds (rows.removeFromTop (26).removeFromRight (84).reduced (0, 2));
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (look::paper);
        auto area = getLocalBounds().reduced (14); area.removeFromRight (130);
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
        g.setFont (juce::FontOptions (11.0f)); int lx = legend.getX();
        for (int b = 0; b < fb::numBands; ++b)
        {
            g.setColour (look::bands[b]); g.fillRect (lx, legend.getY() + 6, 12, 12); g.setColour (look::soft);
            const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), look::bandNames[b]) + 8; g.drawText (look::bandNames[b], lx + 16, legend.getY() + 2, tw, 20, juce::Justification::centredLeft); lx += tw + 26;
        }
    }
private:
    struct Part { juce::String text; bool warn = false; float fixDb = 0; int slot = -1; };
    static constexpr int rowsHeight = 26 * 3 + 34;

    // 30 times a second: read what the Kick Fits say, work out how much of each is in the bus, compare with the measured songs
    void timerCallback() override
    {
        static const char* const names[3] = { "Kick", "Bass", "Vocal" }; static const char* const on[3] = { "The kick", "The bass", "The vocal" };
        const auto& t = kf::modeTargets[(size_t) juce::jlimit (0, kf::numModes - 1, (int) proc.state.getRawParameterValue ("mode")->load())];
        auto* b = fl::shared(); bool have[3] = { false, false, false }; double P[3][fl::numBands] {}, peak[3] {}, B[fl::numBands];
        for (auto& q : part) q.slot = -1;
        if (b != nullptr)
            for (int i = 0; i < fl::maxSlots; ++i)
            {
                const auto& sl = b->slot[i]; const int r = sl.role.load();
                if (! fl::alive (sl) || r < 0 || r > 2 || have[r]) continue;                 // the first of each kind counts
                have[r] = true; part[r].slot = i; peak[r] = sl.peak.load(); for (int k = 0; k < fl::numBands; ++k) P[r][k] = sl.power[k].load();
            }
        if (have[0] != had[0] || have[1] != had[1] || have[2] != had[2]) { mixer.reset(); for (int k = 0; k < 3; ++k) had[k] = have[k]; }
        for (int k = 0; k < fl::numBands; ++k) B[k] = proc.engine.inPower[k].load();
        const double song = proc.engine.inPeak.load(); const bool playing = song > 1e-4;
        if (playing) mixer.add (P, have, B);
        double g2[3], fit = 0; const bool solved = playing && mixer.solve (have, g2, fit), sure = solved && fit >= 0.9;
        if (cool > 0) --cool;
        note = b == nullptr ? "This computer would not let Fit Bus and Kick Fit talk to each other, so Fit Bus only limits and draws."
             : ! (have[0] || have[1] || have[2]) ? "No Kick Fit found. Put Kick Fit on the kick, the bass and the vocal, set each one's \"I am on\", and send those tracks here."
             : ! playing ? "Play the song."
             : ! sure ? "There is more in this bus than the parts with a Kick Fit on them, so their heights can't be read. Send only the kick, bass and vocal here."
             : said.isNotEmpty() && cool > 0 ? said
             : "Heights are measured after the faders. The ranges come from finished songs, where other instruments share the low end: with only these parts playing, trust your ears on the bass.";
        for (int k = 0; k < 3; ++k)
        {
            auto& q = part[k]; q.warn = false; q.fixDb = 0; const float* range = k == 0 ? t.kickPk : k == 1 ? t.bassPk : t.vocPk;
            if (! have[k]) q.text = juce::String (names[k]) + ": no Kick Fit set to \"" + on[k] + "\" is running.";
            else if (! sure) q.text = juce::String (names[k]) + ": found.";
            else if (g2[k] <= 1e-9 || peak[k] <= 1e-6) q.text = juce::String (names[k]) + ": found, but none of it reaches this bus.";
            else
            {
                const float h = (float) (20 * std::log10 (peak[k] * std::sqrt (g2[k]) / song));          // this part's peak against the bus's loudest peak
                q.text = juce::String (names[k]) + ": peaks " + (h < -0.05f ? juce::String (-h, 1) + " dB under" : "at") + " the loudest peak. ";
                if (! t.peaks) q.text << "Not enough " << t.name << " songs measured to compare.";
                else
                {
                    q.text << "Your " << t.name << " songs: " << juce::String (-range[2], 1) << " to " << juce::String (-range[0], 1) << " under. ";
                    if (h > range[2] + 0.5f) { q.warn = true; q.fixDb = range[1] - h; q.text << "Taller than most."; }
                    else if (h < range[0] - 0.5f) { q.warn = true; q.fixDb = range[1] - h; q.text << "Lower than most."; }
                    else q.text << "In range.";
                }
            }
            fix[k].setEnabled (q.fixDb != 0 && cool == 0); fix[k].setButtonText (q.fixDb != 0 ? (q.fixDb > 0 ? "Up " : "Down ") + juce::String (std::abs (q.fixDb), 1) : juce::String ("Fix"));
        }
        repaint();
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
    fb::Mixer mixer; Part part[3]; bool had[3] = { false, false, false }; juce::String note, said; int cool = 0;
};

juce::AudioProcessorEditor* FitBusProcessor::createEditor() { return new FitBusEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FitBusProcessor(); }
