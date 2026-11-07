#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// The plugin window: the full Semitone interface in an embedded web view,
// connected to the native side with a few events (see web bridge in the HTML).
class SemitoneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SemitoneEditor (SemitoneProcessor&);
    ~SemitoneEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    juce::WebBrowserComponent::Options makeOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    void saveFile (const juce::String& name, const juce::String& base64);
    void dragFile (const juce::String& name, const juce::String& base64);
    void dragAudio (const juce::var& v);
    void sendMarkers (const juce::var& v);
    void scanLibrary();
    void sendHostList();
    void loopPut (const juce::var&);
    void loopGet (const juce::String& id);
    void loopSend (const juce::String& id, const juce::File& f, const juce::String& event);
    void clipFile();
    void takeStart();
    void takeOpenInput (bool granted);
    void takeStop();
    void loadLibraryScan();
    void scanDone (const juce::String& json);
    void prepareMarkers();
    void startDrag (const juce::File& f);
    void dragDone (bool ok, const juce::String& why);
    static juce::File dragDir();
    void saveSong (const juce::var& v);
    void openSong();
    void openSongAt (const juce::File& f);
    void songOpened (const juce::File& f, const juce::String& json);
    void setTitle (const juce::String& t);
    void clipChunk (const juce::var& v);
    void toggleFullScreen();
    void showAudioSettings();
    void hideTitleOptions();
    void reveal();
    static juce::MemoryBlock decode (const juce::String& base64);

    SemitoneProcessor& proc;
    juce::WebBrowserComponent browser;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<juce::uint32> incoming;
    juce::String lastAutosaved;
    int lastHostLatency = 0;
    std::atomic<bool> bouncing { false };
    void startBounce (const juce::var&);
    juce::File takeFile;
    bool lastPlaying = false;
    int ticks = 0;
    float meterWas = 0;
    bool shown = false;    // the page has drawn: until then the window shows a loading screen
    juce::Rectangle<int> sizeBeforeFull;
    std::shared_ptr<st::Clip> building;   // the clip arriving in chunks
    juce::String buildingId;
    juce::int64 buildingAt = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SemitoneEditor)
};
