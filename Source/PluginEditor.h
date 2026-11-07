#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "SpectrumView.h"

class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel()
    {
        setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff7a18));
        setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2a2f3d));
        g.strokePath (track, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, angle, true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (10.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float br = r * 0.68f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b3144), c.x, c.y - br,
                                                 juce::Colour (0xff14171f), c.x, c.y + br, false));
        g.fillEllipse (c.x - br, c.y - br, 2.f * br, 2.f * br);

        juce::Path ptr;
        ptr.addRoundedRectangle (-1.5f, -br, 3.f, br * 0.5f, 1.5f);
        g.setColour (juce::Colours::white);
        g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }
};

class PunchBassEditor : public juce::AudioProcessorEditor
{
public:
    explicit PunchBassEditor (PunchBassProcessor&);
    ~PunchBassEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name,
              const juce::String& suffix, juce::Colour colour, juce::Component& parent)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
            slider.setTextValueSuffix (suffix);
            slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
            slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
            slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            label.setText (name, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (juce::Font (12.f, juce::Font::bold));
            attachment = std::make_unique<SliderAtt> (s, id, slider);
            parent.addAndMakeVisible (slider);
            parent.addAndMakeVisible (label);
        }

        void place (juce::Rectangle<int> r)
        {
            label.setBounds (r.removeFromTop (16));
            slider.setBounds (r);
        }

        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> attachment;
    };

    Knob* addKnob (const juce::String& id, const juce::String& name, const juce::String& suffix, juce::Colour c);
    void placeRow (juce::Rectangle<int> panel, std::initializer_list<Knob*> row);

    PunchBassProcessor& proc;
    NeonLookAndFeel laf;
    SpectrumView spectrum;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *punch = nullptr, *tail = nullptr,
         *split = nullptr, *drive = nullptr, *warmth = nullptr, *fat = nullptr,
         *wGain = nullptr, *wFreq = nullptr,
         *mix = nullptr, *outGain = nullptr, *ceilKnob = nullptr;

    juce::ToggleButton monoLow, limOn;
    std::unique_ptr<ButtonAtt> monoAtt, limAtt;

    juce::Rectangle<int> pPunch, pFat, pWeight, pOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchBassEditor)
};
