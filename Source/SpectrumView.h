#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Live input spectrum, reduction curve (violet, hanging from the top) and the
// sensitivity-shaping curve with six draggable nodes (teal).
//   drag      : frequency (x) / sensitivity (y)
//   wheel     : Q / width
//   dbl-click : reset sensitivity to 0
class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView (SereneProcessor&);
    ~SpectrumView() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override { rebuildColumns(); }

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void rebuildColumns();

    juce::Rectangle<float> plot() const;
    float freqToX (float hz) const;
    float xToFreq (float x) const;
    float levelToY (float db) const;
    float reductionToY (float db) const;
    float weightToY (float w) const;
    float yToWeight (float y) const;
    int hitNode (juce::Point<float>) const;

    void setParam (const juce::String& id, float value);
    juce::RangedAudioParameter* param (const juce::String& id) const;

    SereneProcessor& proc;

    std::vector<float> rawLevel, rawRed;
    double binHz = 1.0;
    std::vector<float> colLevel, colRed;   // per-pixel-column, smoothed
    int hoverNode = -1, dragNode = -1;

    static constexpr float minHz = 20.0f, maxHz = 20000.0f;
};
