#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Low-end spectrum 20 Hz - 2 kHz: white = input, orange/red = output,
// plus markers for the split and weight frequencies and a punch activity bar.
class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView (PunchBassProcessor& p) : proc (p) { startTimerHz (30); }
    ~SpectrumView() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0d14), 0.f, 0.f,
                                                 juce::Colour (0xff1a1220), 0.f, bounds.getHeight(), false));
        g.fillRoundedRectangle (bounds, 10.f);

        const auto plot = getPlot();
        drawGrid (g, plot);
        drawSpectra (g, plot);
        drawMarkers (g, plot);
        drawPunchMeter (g, plot);
    }

private:
    static constexpr int N = SpectrumFeed::size;
    static constexpr float kFMin = 20.f, kFMax = 2000.f;

    juce::Rectangle<float> getPlot() const
    {
        return getLocalBounds().toFloat().reduced (12.f, 10.f).withTrimmedBottom (14.f);
    }

    static float freqToX (float f, juce::Rectangle<float> r)
    {
        return r.getX() + r.getWidth() * std::log (juce::jmax (kFMin, f) / kFMin) / std::log (kFMax / kFMin);
    }

    double getSR() const { return juce::jmax (8000.0, proc.getSampleRate()); }

    void timerCallback() override
    {
        if (proc.inFeed.pull (block))  analyse (specIn);
        if (proc.outFeed.pull (block)) analyse (specOut);
        repaint();
    }

    void analyse (std::vector<float>& spec)
    {
        const auto plot = getPlot();
        const int w = (int) plot.getWidth();
        if (w <= 0) return;
        if ((int) spec.size() != w) spec.assign ((size_t) w, -120.f);

        std::copy (block.begin(), block.end(), fftData.begin());
        std::fill (fftData.begin() + N, fftData.end(), 0.f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const float sr = (float) getSR();
        const float norm = juce::Decibels::gainToDecibels ((float) N);
        const float ratio = kFMax / kFMin;

        for (int x = 0; x < w; ++x)
        {
            const float f0 = kFMin * std::pow (ratio, (float) x / (float) w);
            const float f1 = kFMin * std::pow (ratio, (float) (x + 1) / (float) w);
            const float b0 = f0 * (float) N / sr, b1 = f1 * (float) N / sr;

            float mag = 0.f;
            if (b1 - b0 < 1.f)
            {
                const float bc = 0.5f * (b0 + b1);
                const int i0 = juce::jlimit (0, N / 2 - 2, (int) bc);
                const float fr = bc - (float) i0;
                mag = fftData[(size_t) i0] + fr * (fftData[(size_t) i0 + 1] - fftData[(size_t) i0]);
            }
            else
            {
                const int i0 = juce::jlimit (0, N / 2 - 1, (int) std::ceil (b0));
                const int i1 = juce::jlimit (0, N / 2 - 1, (int) std::floor (b1));
                for (int i = i0; i <= i1; ++i) mag = juce::jmax (mag, fftData[(size_t) i]);
            }

            const float db = juce::Decibels::gainToDecibels (mag) - norm + 6.f;
            float& s = spec[(size_t) x];
            s += (db - s) * (db > s ? 0.6f : 0.15f);
        }
    }

    static float dbToY (float db, juce::Rectangle<float> plot)
    {
        return plot.getBottom() - plot.getHeight() * juce::jlimit (0.f, 1.f, (db + 84.f) / 78.f);
    }

    juce::Path makeLine (const std::vector<float>& s, juce::Rectangle<float> plot) const
    {
        juce::Path p;
        for (size_t i = 0; i < s.size(); ++i)
        {
            const float x = plot.getX() + (float) i;
            const float y = dbToY (s[i], plot);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        g.setFont (11.f);
        const float freqs[] = { 30, 50, 100, 200, 500, 1000, 2000 };
        for (float f : freqs)
        {
            const float x = freqToX (f, plot);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            const juce::String t = f >= 1000.f ? juce::String ((int) (f / 1000.f)) + "k" : juce::String ((int) f);
            g.drawText (t, juce::Rectangle<int> ((int) x - 20, (int) plot.getBottom() + 1, 40, 12), juce::Justification::centred);
        }
        for (int i = 1; i < 5; ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i / 5.f), plot.getX(), plot.getRight());
        }
    }

    void drawSpectra (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        if (! specOut.empty())
        {
            auto line = makeLine (specOut, plot);
            juce::Path fill (line);
            fill.lineTo (plot.getX() + (float) specOut.size() - 1.f, plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff2e63).withAlpha (0.6f), 0.f, plot.getY(),
                                                     juce::Colour (0xffff7a18).withAlpha (0.05f), 0.f, plot.getBottom(), false));
            g.fillPath (fill);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff7a18), plot.getX(), 0.f,
                                                     juce::Colour (0xffffd166), plot.getRight(), 0.f, false));
            g.strokePath (line, juce::PathStrokeType (2.f));
        }
        if (! specIn.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.strokePath (makeLine (specIn, plot), juce::PathStrokeType (1.2f));
        }

        g.setFont (juce::Font (12.f, juce::Font::bold));
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText ("IN", juce::Rectangle<int> ((int) plot.getRight() - 90, (int) plot.getY() + 4, 40, 14), juce::Justification::right);
        g.setColour (juce::Colour (0xffff7a18));
        g.drawText ("OUT", juce::Rectangle<int> ((int) plot.getRight() - 46, (int) plot.getY() + 4, 40, 14), juce::Justification::right);
    }

    void drawMarkers (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        const float split = proc.apvts.getRawParameterValue ("split")->load();
        const float wf    = proc.apvts.getRawParameterValue ("w_freq")->load();
        const float dashes[] = { 4.f, 4.f };

        g.setFont (10.f);
        const float xs = freqToX (split, plot);
        g.setColour (juce::Colour (0xffff7a18).withAlpha (0.8f));
        g.drawDashedLine (juce::Line<float> (xs, plot.getY(), xs, plot.getBottom()), dashes, 2, 1.f);
        g.drawText ("SPLIT", juce::Rectangle<int> ((int) xs + 3, (int) plot.getBottom() - 14, 40, 12), juce::Justification::left);

        const float xw = freqToX (wf, plot);
        g.setColour (juce::Colour (0xff2de2e6).withAlpha (0.8f));
        g.drawDashedLine (juce::Line<float> (xw, plot.getY(), xw, plot.getBottom()), dashes, 2, 1.f);
        g.drawText ("WEIGHT", juce::Rectangle<int> ((int) xw + 3, (int) plot.getBottom() - 28, 50, 12), juce::Justification::left);
    }

    void drawPunchMeter (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        const float gdb = proc.shaperGainDb.load();
        auto area = juce::Rectangle<float> (plot.getX() + 8.f, plot.getY() + 8.f, 140.f, 10.f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (area, 5.f);

        const float amt = juce::jlimit (0.f, 1.f, std::abs (gdb) / 18.f);
        g.setColour (gdb >= 0.f ? juce::Colour (0xffff2e63) : juce::Colour (0xff2de2e6));
        g.fillRoundedRectangle (area.withWidth (juce::jmax (2.f, area.getWidth() * amt)), 5.f);

        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("PUNCH " + juce::String (gdb, 1) + " dB",
                    juce::Rectangle<int> ((int) area.getX(), (int) area.getBottom() + 3, 160, 14), juce::Justification::left);
    }

    PunchBassProcessor& proc;
    juce::dsp::FFT fft { SpectrumFeed::order };
    juce::dsp::WindowingFunction<float> window { (size_t) N, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, N> block {};
    std::array<float, 2 * N> fftData {};
    std::vector<float> specIn, specOut;
};
