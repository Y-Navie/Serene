#include "PluginEditor.h"

SereneEditor::SereneEditor (SereneProcessor& p)
    : AudioProcessorEditor (p), proc (p), spectrum (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (spectrum);

    setupKnob (depth,   ids::depth,   "DEPTH",   colours::reduction);
    setupKnob (detail,  ids::detail,  "DETAIL",  colours::accent);
    setupKnob (attack,  ids::attack,  "ATTACK",  colours::accent);
    setupKnob (release, ids::release, "RELEASE", colours::accent);
    setupKnob (mix,     ids::mix,     "MIX",     colours::text);
    setupKnob (trim,    ids::trim,    "TRIM",    colours::text);

    setupCombo (modeBox,    modeAtt,    ids::mode);
    setupCombo (qualityBox, qualityAtt, ids::quality);
    setupCombo (channelBox, channelAtt, ids::channels);

    for (auto* b : { &deltaBtn, &bypassBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    deltaBtn.setColour (juce::TextButton::buttonOnColourId, colours::reduction);
    deltaBtn.setTooltip ("Listen only to what is being removed");
    deltaAtt  = std::make_unique<BA> (proc.apvts, ids::delta,  deltaBtn);
    bypassAtt = std::make_unique<BA> (proc.apvts, ids::bypass, bypassBtn);

    setResizable (true, true);
    setResizeLimits (720, 440, 1600, 1000);
    setSize (920, 540);
}

SereneEditor::~SereneEditor() { setLookAndFeel (nullptr); }

void SereneEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name, juce::Colour col)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, col);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                  proc.apvts.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (k.label);

    k.att = std::make_unique<SA> (proc.apvts, id, k.slider);
}

void SereneEditor::setupCombo (juce::ComboBox& box, std::unique_ptr<CA>& att, const juce::String& id)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        box.addItemList (choice->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<CA> (proc.apvts, id, box);
}

void SereneEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    auto header = getLocalBounds().removeFromTop (48).reduced (16, 0);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("SERENE", header, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("dynamic resonance suppressor", header.withTrimmedLeft (102), juce::Justification::centredLeft);
}

void SereneEditor::resized()
{
    auto r = getLocalBounds();

    auto header = r.removeFromTop (48).reduced (12, 9);
    bypassBtn.setBounds (header.removeFromRight (78));
    header.removeFromRight (8);
    deltaBtn.setBounds (header.removeFromRight (70));
    header.removeFromRight (14);
    channelBox.setBounds (header.removeFromRight (110));
    header.removeFromRight (8);
    qualityBox.setBounds (header.removeFromRight (120));
    header.removeFromRight (8);
    modeBox.setBounds (header.removeFromRight (80));

    auto knobs = r.removeFromBottom (std::max (120, getHeight() / 4)).reduced (12, 6);
    spectrum.setBounds (r.reduced (12, 0));

    Knob* all[] = { &depth, &detail, &attack, &release, &mix, &trim };
    const int w = knobs.getWidth() / 6;
    for (auto* k : all)
    {
        auto cell = knobs.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (18));
        k->slider.setBounds (cell);
    }
}
