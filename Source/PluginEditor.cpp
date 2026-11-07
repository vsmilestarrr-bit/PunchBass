#include "PluginEditor.h"

PunchBassEditor::Knob* PunchBassEditor::addKnob (const juce::String& id, const juce::String& name,
                                                 const juce::String& suffix, juce::Colour c)
{
    knobs.push_back (std::make_unique<Knob> (proc.apvts, id, name, suffix, c, *this));
    return knobs.back().get();
}

PunchBassEditor::PunchBassEditor (PunchBassProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (spectrum);

    const juce::Colour red (0xffff2e63), orange (0xffff7a18), yellow (0xffffd166), cyan (0xff2de2e6);

    punch  = addKnob ("punch",  "PUNCH", " %", red);
    tail   = addKnob ("tail",   "TAIL",  " %", red);

    split  = addKnob ("split",  "SPLIT",  " Hz", orange);
    drive  = addKnob ("drive",  "DRIVE",  " dB", orange);
    warmth = addKnob ("warmth", "WARMTH", " %",  orange);
    fat    = addKnob ("fat",    "FAT",    " %",  orange);

    wGain  = addKnob ("w_gain", "WEIGHT", " dB", yellow);
    wFreq  = addKnob ("w_freq", "FREQ",   " Hz", yellow);

    mix      = addKnob ("mix",      "MIX",     " %",  cyan);
    outGain  = addKnob ("out_gain", "OUTPUT",  " dB", cyan);
    ceilKnob = addKnob ("lim_ceil", "CEILING", " dB", cyan);

    auto setupToggle = [this] (juce::ToggleButton& b, const juce::String& text, const juce::String& id,
                               std::unique_ptr<ButtonAtt>& att)
    {
        b.setButtonText (text);
        addAndMakeVisible (b);
        att = std::make_unique<ButtonAtt> (proc.apvts, id, b);
    };
    setupToggle (monoLow, "Mono Bass", "mono_low", monoAtt);
    setupToggle (limOn,   "Limiter",   "lim_on",   limAtt);

    setSize (1100, 520);
}

PunchBassEditor::~PunchBassEditor()
{
    setLookAndFeel (nullptr);
}

void PunchBassEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff090a10), 0.f, 0.f,
                                             juce::Colour (0xff15101c), 0.f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (22.f, juce::Font::bold));
    g.drawText ("PUNCHBASS", 14, 10, 220, 30, juce::Justification::left);
    g.setColour (juce::Colour (0xffff7a18));
    g.setFont (11.f);
    g.drawText ("KICK & BASS ENHANCER", 14, 34, 300, 12, juce::Justification::left);

    auto drawPanel = [&] (juce::Rectangle<int> r, const juce::String& title, juce::Colour c)
    {
        g.setColour (juce::Colour (0xff171a24));
        g.fillRoundedRectangle (r.toFloat(), 10.f);
        g.setColour (c.withAlpha (0.35f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.f, 1.f);
        g.setColour (c);
        g.setFont (juce::Font (13.f, juce::Font::bold));
        g.drawText (title, r.reduced (12, 6).removeFromTop (18), juce::Justification::left);
    };

    drawPanel (pPunch,  "PUNCH",  juce::Colour (0xffff2e63));
    drawPanel (pFat,    "FAT",    juce::Colour (0xffff7a18));
    drawPanel (pWeight, "WEIGHT", juce::Colour (0xffffd166));
    drawPanel (pOut,    "OUTPUT", juce::Colour (0xff2de2e6));
}

void PunchBassEditor::placeRow (juce::Rectangle<int> panel, std::initializer_list<Knob*> row)
{
    auto a = panel.reduced (10);
    a.removeFromTop (22);
    const int cw = a.getWidth() / (int) row.size();
    for (auto* k : row)
        k->place (a.removeFromLeft (cw));
}

void PunchBassEditor::resized()
{
    auto r = getLocalBounds().reduced (10);

    auto header = r.removeFromTop (34);
    for (auto* t : { &limOn, &monoLow })
        t->setBounds (header.removeFromRight (130));

    spectrum.setBounds (r.removeFromTop (250));
    r.removeFromTop (8);

    const int avail = r.getWidth() - 3 * 8;
    auto take = [&] (int units) { return r.removeFromLeft (avail * units / 11); };

    pPunch = take (2);  r.removeFromLeft (8);
    pFat   = take (4);  r.removeFromLeft (8);
    pWeight = take (2); r.removeFromLeft (8);
    pOut   = r;

    placeRow (pPunch,  { punch, tail });
    placeRow (pFat,    { split, drive, warmth, fat });
    placeRow (pWeight, { wGain, wFreq });
    placeRow (pOut,    { mix, outGain, ceilKnob });
}
