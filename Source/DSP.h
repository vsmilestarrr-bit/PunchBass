#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// Spectrum feed (audio thread writes, GUI reads)
//==============================================================================
struct SpectrumFeed
{
    static constexpr int order   = 13;
    static constexpr int size    = 1 << order;
    static constexpr int hopSize = size / 4;

    void push (float s)
    {
        ring[(size_t) w] = s;
        w = (w + 1) & (size - 1);
        if (++count >= hopSize)
        {
            count = 0;
            if (! ready.load())
            {
                for (int i = 0; i < size; ++i)
                    block[(size_t) i] = ring[(size_t) ((w + i) & (size - 1))];
                ready = true;
            }
        }
    }

    bool pull (std::array<float, size>& dest)
    {
        if (! ready.load()) return false;
        dest = block;
        ready = false;
        return true;
    }

    std::array<float, size> ring {}, block {};
    int w = 0, count = 0;
    std::atomic<bool> ready { false };
};

//==============================================================================
// Transient shaper: differential envelope (fast vs slow)
//==============================================================================
class TransientShaper
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aF = coef (0.5f);  rF = coef (12.f);
        aS = coef (18.f);  rS = coef (120.f);
        smC = coef (0.5f);
        envF = envS = 0.f;
        gSm = 0.f;
    }

    // returns linear gain
    float process (float level, float punch, float tail)
    {
        envF = (level > envF ? aF : rF) * envF + (1.f - (level > envF ? aF : rF)) * level;
        envS = (level > envS ? aS : rS) * envS + (1.f - (level > envS ? aS : rS)) * level;

        const float d = 20.f * std::log10 ((envF + 1e-5f) / (envS + 1e-5f));
        const float target = punch * 1.5f * juce::jlimit (0.f, 12.f, d)
                           + tail * juce::jlimit (0.f, 12.f, -d);

        gSm = smC * gSm + (1.f - smC) * target;
        return juce::Decibels::decibelsToGain (gSm);
    }

    float getGainDb() const { return gSm; }

private:
    float coef (float ms) const { return std::exp (-1.f / (ms * 0.001f * (float) fs)); }
    double fs = 44100.0;
    float aF = 0.f, rF = 0.f, aS = 0.f, rS = 0.f, smC = 0.f;
    float envF = 0.f, envS = 0.f, gSm = 0.f;
};

//==============================================================================
// Bass enhancer: LR4 split -> low band tanh saturation (+even harmonics via bias)
// -> DC blocker -> sub "weight" peak filter -> sum with highs
//==============================================================================
class BassEnhancer
{
public:
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using Filter = juce::dsp::IIR::Filter<float>;

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
        xo.prepare (spec);
        xo.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        xo.reset();
        lastSplit = -1.f;

        for (int c = 0; c < 2; ++c)
        {
            wf[c].coefficients = Coeffs::makePeakFilter (fs, 55.0, 0.9f, 1.0f);
            wf[c].reset();
            dcX[c] = dcY[c] = 0.f;
        }
        lastWF = lastWG = -1.f;
    }

    void setParams (float split, float driveDb, float warmth01, float fat01,
                    float weightDb, float weightFreq, bool mono)
    {
        split = juce::jlimit (30.f, (float) (fs * 0.3), split);
        if (split != lastSplit) { xo.setCutoffFrequency (split); lastSplit = split; }

        drive = juce::Decibels::decibelsToGain (driveDb);
        bias = warmth01 * 0.6f;
        biasT = std::tanh (bias);
        invSqrtDrive = 1.f / std::sqrt (drive);
        fat = fat01;
        monoLow = mono;

        if (weightFreq != lastWF || weightDb != lastWG)
        {
            auto c = Coeffs::makePeakFilter (fs, (double) weightFreq, 0.9f, juce::Decibels::decibelsToGain (weightDb));
            wf[0].coefficients = c;
            wf[1].coefficients = c;
            lastWF = weightFreq; lastWG = weightDb;
        }
    }

    void processSample (float& l, float& r)
    {
        float x[2] = { l, r }, lo[2], hi[2], out[2];
        for (int c = 0; c < 2; ++c)
            xo.processSample (c, x[c], lo[c], hi[c]);

        if (monoLow)
        {
            const float m = 0.5f * (lo[0] + lo[1]);
            lo[0] = lo[1] = m;
        }

        for (int c = 0; c < 2; ++c)
        {
            float s = (std::tanh (drive * lo[c] + bias) - biasT) * invSqrtDrive;
            const float y = s - dcX[c] + 0.9995f * dcY[c];      // DC blocker (~4 Hz)
            dcX[c] = s;
            dcY[c] = y;

            float wet = lo[c] + fat * (y - lo[c]);
            wet = wf[c].processSample (wet);
            out[c] = wet + hi[c];
        }
        l = out[0];
        r = out[1];
    }

private:
    double fs = 44100.0;
    juce::dsp::LinkwitzRileyFilter<float> xo;
    Filter wf[2];
    float dcX[2] {}, dcY[2] {};
    float drive = 1.f, bias = 0.f, biasT = 0.f, invSqrtDrive = 1.f, fat = 0.f;
    float lastSplit = -1.f, lastWF = -1.f, lastWG = -1.f;
    bool monoLow = false;
};

//==============================================================================
// Output gain + safety limiter
//==============================================================================
class OutputLimiter
{
public:
    void prepare (double sr) { relC = std::exp (-1.f / (0.12f * (float) sr)); g = 1.f; }

    void process (juce::AudioBuffer<float>& buf, float gainDb, bool limitOn, float ceilDb)
    {
        const int nCh = juce::jmin (buf.getNumChannels(), 2);
        const int n = buf.getNumSamples();
        if (nCh == 0) return;
        const float gain = juce::Decibels::decibelsToGain (gainDb);
        const float ceil = juce::Decibels::decibelsToGain (ceilDb);
        float* d[2] = { buf.getWritePointer (0), nCh > 1 ? buf.getWritePointer (1) : nullptr };

        for (int i = 0; i < n; ++i)
        {
            float peak = 0.f;
            for (int c = 0; c < nCh; ++c) { d[c][i] *= gain; peak = juce::jmax (peak, std::abs (d[c][i])); }
            if (! limitOn) continue;

            const float target = peak > ceil ? ceil / peak : 1.f;
            if (target < g) g = target;
            else            g = relC * g + (1.f - relC) * target;
            for (int c = 0; c < nCh; ++c) d[c][i] = juce::jlimit (-ceil, ceil, d[c][i] * g);
        }
    }

private:
    float g = 1.f, relC = 0.999f;
};
