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
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xffE8ECE5)); }

private:
    void timerCallback() override;
    juce::WebBrowserComponent::Options makeOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    void saveFile (const juce::String& name, const juce::String& base64);
    void dragFile (const juce::String& name, const juce::String& base64);
    static juce::MemoryBlock decode (const juce::String& base64);

    SemitoneProcessor& proc;
    juce::WebBrowserComponent browser;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<juce::uint32> incoming;
    bool lastPlaying = false;
    int ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SemitoneEditor)
};
