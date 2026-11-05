#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Sequencer.h"
#include "ClipPlayer.h"
#include "Synth.h"
#include <map>
#include <set>
#include <array>

// The plugin: plays the song from the Semitone interface as MIDI, in time with the host.
// Each part leaves on its own MIDI channel so the DAW can route it to any instrument (Omnisphere etc).
class SemitoneProcessor : public juce::AudioProcessor
{
public:
    SemitoneProcessor();

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Semitone Studio"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; } // note releases and the room

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
    // ---- Semitone's own sounds inside the DAW (message thread) ----
    // soundMode: 0 = Semitone's sounds + MIDI, 1 = Semitone's sounds only, 2 = MIDI only,
    // 3 = the built-in engine alone (ignores rendered sections, no MIDI out)
    void setSoundMode (int m) { soundMode = juce::jlimit (0, 3, m); }
    int getSoundMode() const { return soundMode.load(); }
    // layout: [{id, at}] — returns the clip ids this plugin still needs rendered
    juce::StringArray setAudioLayout (const juce::var& items);
    void addClip (const juce::String& id, std::shared_ptr<st::Clip> clip);
    void releaseRetired();
    double getSampleRateNow() const { return sr; }
    juce::String songFile; // the .semitone file this song was last opened from or saved to (message thread)

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

    st::ClipPlayer clipPlayer;
    std::atomic<int> soundMode { 0 };
    std::atomic<double> tlLength { 0.0 }, tlOffset { 0.0 };
    std::map<juce::String, std::shared_ptr<st::Clip>> clips;   // message thread
    juce::var audioLayout;                                       // message thread
    std::vector<std::pair<std::shared_ptr<const st::Bank>, juce::uint32>> retired;
    std::shared_ptr<const st::Bank> bankNow;
    void rebuildBank();
    std::shared_ptr<st::Clip> loadCachedClip (const juce::String& id);

    // the native engine: plays whatever no rendered section covers
    st::Synth synth;
    void setSynthSound (const juce::var& sound, const st::Timeline& tl);
    void buildSynthPatch();
    juce::CriticalSection synthLock;                               // guards the next two
    std::shared_ptr<const st::SynthPatch> synthTemplate;
    std::vector<std::set<int>> synthKeys;                          // per part: the notes the song uses
    std::atomic<int> synthGen { 0 };
    std::map<juce::int64, std::unique_ptr<st::NoteBuffer>> noteCache;   // the build thread only; voices read it, so it only grows
    std::vector<std::pair<std::shared_ptr<const st::SynthPatch>, juce::uint32>> retiredPatches;   // the build thread only
    std::shared_ptr<const st::SynthPatch> patchNow;                // the build thread only

    juce::AbstractFifo inFifo { 1024 };
    std::array<juce::uint32, 1024> inBuf {};

    juce::CriticalSection stateLock;
    juce::String projectJson, timelineJson;

    juce::ThreadPool synthPool { 1 };   // last, so it stops first

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SemitoneProcessor)
};
