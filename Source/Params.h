#pragma once
#include <JuceHeader.h>

namespace Params
{
    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        using PID = ParameterID;
        using Range = NormalisableRange<float>;

        AudioProcessorValueTreeState::ParameterLayout l;

        // Punch (transient shaper)
        l.add (std::make_unique<AudioParameterFloat> (PID { "punch", 1 }, "Punch", Range (0.f, 100.f, 0.1f), 50.f));
        l.add (std::make_unique<AudioParameterFloat> (PID { "tail", 1 },  "Tail",  Range (-100.f, 100.f, 0.1f), 0.f));

        // Fat (low band saturation)
        l.add (std::make_unique<AudioParameterFloat> (PID { "split", 1 },  "Split",  Range (60.f, 300.f, 1.f, 0.6f), 140.f));
        l.add (std::make_unique<AudioParameterFloat> (PID { "drive", 1 },  "Drive",  Range (0.f, 24.f, 0.1f), 8.f));
        l.add (std::make_unique<AudioParameterFloat> (PID { "warmth", 1 }, "Warmth", Range (0.f, 100.f, 0.1f), 30.f));
        l.add (std::make_unique<AudioParameterFloat> (PID { "fat", 1 },    "Fat",    Range (0.f, 100.f, 0.1f), 60.f));
        l.add (std::make_unique<AudioParameterBool>  (PID { "mono_low", 1 }, "Mono Bass", false));

        // Weight (sub boost)
        l.add (std::make_unique<AudioParameterFloat> (PID { "w_gain", 1 }, "Weight",      Range (0.f, 12.f, 0.1f), 3.f));
        l.add (std::make_unique<AudioParameterFloat> (PID { "w_freq", 1 }, "Weight Freq", Range (35.f, 120.f, 0.5f, 0.7f), 55.f));

        // Output
        l.add (std::make_unique<AudioParameterFloat> (PID { "mix", 1 },      "Mix",     Range (0.f, 100.f, 0.1f), 100.f));
        l.add (std::make_unique<AudioParameterFloat> (PID { "out_gain", 1 }, "Output",  Range (-12.f, 12.f, 0.1f), 0.f));
        l.add (std::make_unique<AudioParameterBool>  (PID { "lim_on", 1 },   "Limiter On", true));
        l.add (std::make_unique<AudioParameterFloat> (PID { "lim_ceil", 1 }, "Ceiling", Range (-12.f, 0.f, 0.1f), -0.3f));

        return l;
    }
}
