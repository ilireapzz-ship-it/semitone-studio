#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Sequencer.h"
#include <array>

// The plugin: plays the song from the Semitone interface as MIDI, in time with the host.
// Each part leaves on its own MIDI channel so the DAW can route it to any instrument (Omnisphere etc).
class SemitoneProcessor : public juce::AudioProcessor
{
public:
    SemitoneProcessor();

    void prepareToPlay (double sampleRate, int) override { sr = sampleRate; }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Semitone Studio"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- called from the editor (message thread) ----
    void setTimelineFromVar (const juce::var& v);
    void setProjectState (const juce::String& json) { const juce::ScopedLock sl (stateLock); projectJson = json; }
    juce::String getProjectState() const { const juce::ScopedLock sl (stateLock); return projectJson; }
    void setStandaloneTransport (bool play, double startBeat, double bpm);
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }

    // ---- read by the editor's timer ----
    std::atomic<bool> uiPlaying { false };
    std::atomic<double> uiPpq { 0.0 }, uiBpm { 120.0 };
    std::atomic<bool> uiHostHasTransport { false };

    // MIDI played into the plugin (your keyboard, via the DAW), handed to the interface for
    // play-along, ear training and live recording. Audio thread writes, message thread reads.
    int popIncomingMidi (std::vector<juce::uint32>& dest);

private:
    st::Sequencer sequencer;
    std::vector<st::Event> events;
    double sr = 44100.0;

    // standalone app: Semitone runs its own clock
    std::atomic<bool> stPlaying { false };
    std::atomic<double> stBpm { 120.0 };
    double stPpq = 0.0;
    std::atomic<double> stStartRequest { -1.0 };

    juce::AbstractFifo inFifo { 1024 };
    std::array<juce::uint32, 1024> inBuf {};

    juce::CriticalSection stateLock;
    juce::String projectJson, timelineJson;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SemitoneProcessor)
};
