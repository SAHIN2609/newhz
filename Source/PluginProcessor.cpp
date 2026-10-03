#include "PluginProcessor.h"
#include "PluginEditor.h"

SHZProcessor::SHZProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", makeParameterLayout())
{
    for (int i = 0; i < Id::Count; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (kParams[i].id);
}

void SHZProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int latency = engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (latency);
}

bool SHZProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = l.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void SHZProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = juce::jmin (getTotalNumInputChannels(), 2);
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    for (int i = 0; i < Id::Count; ++i)
        snapshot[(size_t) i] = raw[(size_t) i]->load();

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, numIn, snapshot.data());
}

juce::AudioProcessorEditor* SHZProcessor::createEditor() { return new SHZEditor (*this); }

void SHZProcessor::loadIRFile (const juce::File& f)
{
    if (! f.existsAsFile())
        return;
    engine.loadIR (f);
    irPath = f.getFullPathName();
}

void SHZProcessor::clearIR()
{
    engine.clearIR();
    irPath = {};
}

void SHZProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("irPath", irPath, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SHZProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            const auto path = vt.getProperty ("irPath").toString();
            apvts.replaceState (vt);
            if (path.isNotEmpty() && juce::File (path).existsAsFile())
                loadIRFile (juce::File (path));
            else
                clearIR();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SHZProcessor(); }
