#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace colours
{
inline const juce::Colour bg        { 0xff101216 };
inline const juce::Colour panel     { 0xff181b21 };
inline const juce::Colour outline   { 0xff2a2f38 };
inline const juce::Colour text      { 0xffd9dee7 };
inline const juce::Colour textDim   { 0xff7d8594 };
inline const juce::Colour accent    { 0xff6fe0c8 };   // teal  - knobs / sensitivity curve
inline const juce::Colour reduction { 0xffb48cff };   // violet - reduction
inline const juce::Colour spectrum  { 0xff4a5568 };
}

class SereneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SereneLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, colours::bg);
        setColour (juce::Label::textColourId, colours::text);
        setColour (juce::ComboBox::backgroundColourId, colours::panel);
        setColour (juce::ComboBox::outlineColourId, colours::outline);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::arrowColourId, colours::textDim);
        setColour (juce::PopupMenu::backgroundColourId, colours::panel);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
        setColour (juce::TextButton::buttonColourId, colours::panel);
        setColour (juce::TextButton::buttonOnColourId, colours::accent.withAlpha (0.85f));
        setColour (juce::TextButton::textColourOffId, colours::textDim);
        setColour (juce::TextButton::textColourOnId, colours::bg);
        setColour (juce::Slider::textBoxTextColourId, colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float angle = start + pos * (end - start);
        const float thick = std::max (3.0f, r * 0.12f);
        const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r - thick, r - thick, 0.0f, start, end, true);
        g.setColour (colours::outline);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, r - thick, r - thick, 0.0f, start, angle, true);
        g.setColour (col);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float kr = r - thick * 2.4f;
        g.setColour (colours::panel.brighter (0.08f));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (colours::outline);
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

        const auto tip = c.getPointOnCircumference (kr * 0.8f, angle);
        const auto mid = c.getPointOnCircumference (kr * 0.35f, angle);
        g.setColour (colours::text);
        g.drawLine ({ mid, tip }, 2.5f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto col = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId)
                                      : b.findColour (juce::TextButton::buttonColourId);
        if (over || down) col = col.brighter (0.1f);
        g.setColour (col);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return juce::Font (juce::FontOptions ((float) h * 0.45f, juce::Font::bold));
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
        g.setColour (cb.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (cb.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        juce::Path arrow;
        const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (cb.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox& cb) override
    {
        return juce::Font (juce::FontOptions (std::min (14.0f, (float) cb.getHeight() * 0.5f)));
    }
};
