#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <limits>
#include <optional>
#include "PluginProcessor.h"

class SHZEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SHZEditor (SHZProcessor&);
    ~SHZEditor() override;

    void resized() override;
    void timerCallback() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    juce::RangedAudioParameter* findParam (const juce::String& id);
    void setParamValue (const juce::String& id, float v);

    SHZProcessor& proc;

    std::array<std::atomic<float>*, Id::Count> rawPtrs {};
    std::array<float, Id::Count> lastSent {};

    std::unique_ptr<juce::WebBrowserComponent> web;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SHZEditor)
};
