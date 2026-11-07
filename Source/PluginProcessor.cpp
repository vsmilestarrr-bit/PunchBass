#include "PluginProcessor.h"
#include "PluginEditor.h"

PunchBassProcessor::PunchBassProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", Params::createLayout())
{
}

void PunchBassProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    shaper.prepare (sampleRate);
    enhancer.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
    setLatencySamples (0);
}

bool PunchBassProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void PunchBassProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    if (nCh == 0) return;
    float* d0 = buffer.getWritePointer (0);
    float* d1 = nCh > 1 ? buffer.getWritePointer (1) : nullptr;

    const float punch = get ("punch") * 0.01f;
    const float tail  = get ("tail")  * 0.01f;
    const float mix   = get ("mix")   * 0.01f;

    enhancer.setParams (get ("split"), get ("drive"), get ("warmth") * 0.01f, get ("fat") * 0.01f,
                        get ("w_gain"), get ("w_freq"), get ("mono_low") > 0.5f);

    for (int i = 0; i < n; ++i)
    {
        float l = d0[i];
        float r = d1 != nullptr ? d1[i] : l;
        const float dryL = l, dryR = r;

        inFeed.push (0.5f * (dryL + dryR));

        const float g = shaper.process (juce::jmax (std::abs (l), std::abs (r)), punch, tail);
        l *= g;
        r *= g;

        enhancer.processSample (l, r);

        l = dryL + mix * (l - dryL);
        r = dryR + mix * (r - dryR);

        d0[i] = l;
        if (d1 != nullptr) d1[i] = r;
    }

    limiter.process (buffer, get ("out_gain"), get ("lim_on") > 0.5f, get ("lim_ceil"));

    const float* o0 = buffer.getReadPointer (0);
    const float* o1 = buffer.getReadPointer (nCh > 1 ? 1 : 0);
    for (int i = 0; i < n; ++i)
        outFeed.push (0.5f * (o0[i] + o1[i]));

    shaperGainDb = shaper.getGainDb();
}

juce::AudioProcessorEditor* PunchBassProcessor::createEditor() { return new PunchBassEditor (*this); }

void PunchBassProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PunchBassProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PunchBassProcessor(); }
