#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "Params.h"
#include "DSP.h"

class PunchBassProcessor : public juce::AudioProcessor
{
public:
    PunchBassProcessor();
    ~PunchBassProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PunchBass"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    SpectrumFeed inFeed, outFeed;
    std::atomic<float> shaperGainDb { 0.f };

private:
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    TransientShaper shaper;
    BassEnhancer    enhancer;
    OutputLimiter   limiter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchBassProcessor)
};
