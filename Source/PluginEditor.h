#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumView.h"
#include "SereneLookAndFeel.h"

class SereneEditor : public juce::AudioProcessorEditor
{
public:
    explicit SereneEditor (SereneProcessor&);
    ~SereneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SA> att;
    };

    void setupKnob (Knob&, const juce::String& id, const juce::String& name, juce::Colour);
    void setupCombo (juce::ComboBox&, std::unique_ptr<CA>&, const juce::String& id);

    SereneProcessor& proc;
    SereneLookAndFeel lnf;

    SpectrumView spectrum;
    Knob depth, detail, attack, release, mix, trim;

    juce::ComboBox modeBox, qualityBox, channelBox;
    std::unique_ptr<CA> modeAtt, qualityAtt, channelAtt;

    juce::TextButton deltaBtn { "DELTA" }, bypassBtn { "BYPASS" };
    std::unique_ptr<BA> deltaAtt, bypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SereneEditor)
};
