#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "Params.h"
#include "Engine.h"

class SHZProcessor : public juce::AudioProcessor
{
public:
    SHZProcessor();
    ~SHZProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SISHHIN HZ MACHINE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // user impulse response (message thread)
    void loadIRFile (const juce::File& f);
    void clearIR();
    juce::String getIRName() const { return irPath.isEmpty() ? juce::String() : juce::File (irPath).getFileName(); }

    juce::AudioProcessorValueTreeState apvts;
    shz::Engine engine;

private:
    std::array<std::atomic<float>*, Id::Count> raw {};
    std::array<float, Id::Count> snapshot {};
    juce::String irPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SHZProcessor)
};
