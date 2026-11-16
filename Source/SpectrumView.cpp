#include "SpectrumView.h"
#include "SereneLookAndFeel.h"

SpectrumView::SpectrumView (SereneProcessor& p) : proc (p)
{
    setRepaintsOnMouseActivity (false);
    startTimerHz (40);
}

juce::Rectangle<float> SpectrumView::plot() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedBottom (16.0f);
}

float SpectrumView::freqToX (float hz) const
{
    auto p = plot();
    const float t = std::log (hz / minHz) / std::log (maxHz / minHz);
    return p.getX() + t * p.getWidth();
}

float SpectrumView::xToFreq (float x) const
{
    auto p = plot();
    const float t = (x - p.getX()) / p.getWidth();
    return minHz * std::pow (maxHz / minHz, t);
}

float SpectrumView::levelToY (float db) const
{
    auto p = plot();
    const float t = juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f);   // -90..0 dBFS
    return p.getBottom() - t * p.getHeight();
}

float SpectrumView::reductionToY (float db) const
{
    auto p = plot();
    return p.getY() + juce::jlimit (0.0f, 1.0f, db / 30.0f) * p.getHeight() * 0.6f;
}

float SpectrumView::weightToY (float w) const
{
    auto p = plot();
    return p.getCentreY() - (w / 12.0f) * p.getHeight() * 0.38f;
}

float SpectrumView::yToWeight (float y) const
{
    auto p = plot();
    return juce::jlimit (-12.0f, 12.0f, (p.getCentreY() - y) / (p.getHeight() * 0.38f) * 12.0f);
}

void SpectrumView::rebuildColumns()
{
    const auto w = (size_t) std::max (1, (int) plot().getWidth());
    colLevel.assign (w, -120.0f);
    colRed.assign (w, 0.0f);
}

void SpectrumView::timerCallback()
{
    const bool fresh = proc.fetchDisplay (rawLevel, rawRed, binHz);
    const int nb = (int) rawLevel.size();
    auto p = plot();

    for (size_t c = 0; c < colLevel.size(); ++c)
    {
        float lvl = -120.0f, red = 0.0f;

        if (fresh && nb > 1)
        {
            const float x = p.getX() + (float) c;
            const float b0 = xToFreq (x - 0.5f) / (float) binHz;
            const float b1 = xToFreq (x + 0.5f) / (float) binHz;

            if (b1 - b0 < 1.0f)   // less than one bin per pixel: interpolate
            {
                const float b = juce::jlimit (0.0f, (float) (nb - 1), 0.5f * (b0 + b1));
                const int i = std::min ((int) b, nb - 2);
                const float f = b - (float) i;
                lvl = rawLevel[(size_t) i] + f * (rawLevel[(size_t) i + 1] - rawLevel[(size_t) i]);
                red = rawRed[(size_t) i]   + f * (rawRed[(size_t) i + 1]   - rawRed[(size_t) i]);
            }
            else                  // many bins per pixel: take the max
            {
                const int i0 = juce::jlimit (0, nb - 1, (int) b0), i1 = juce::jlimit (0, nb - 1, (int) b1);
                for (int i = i0; i <= i1; ++i)
                {
                    lvl = std::max (lvl, rawLevel[(size_t) i]);
                    red = std::max (red, rawRed[(size_t) i]);
                }
            }
        }

        auto& L = colLevel[c];
        auto& R = colRed[c];
        L = lvl > L ? 0.5f * (L + lvl) : L + 0.25f * (lvl - L);   // fast up, slower fall
        R = red > R ? 0.6f * red + 0.4f * R : R + 0.2f * (red - R);
    }
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto p = plot();

    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);

    // grid
    g.setFont (juce::FontOptions (11.0f));
    for (float hz : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = freqToX (hz);
        g.setColour (colours::outline.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
        g.setColour (colours::textDim);
        const juce::String label = hz >= 1000.0f ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz);
        g.drawText (label, juce::Rectangle<float> (x - 20, p.getBottom() + 2, 40, 14), juce::Justification::centred);
    }
    for (float db = -12.0f; db >= -84.0f; db -= 12.0f)
    {
        g.setColour (colours::outline.withAlpha (0.35f));
        g.drawHorizontalLine ((int) levelToY (db), p.getX(), p.getRight());
    }

    const int nCols = (int) colLevel.size();
    if (nCols > 1)
    {
        // input spectrum
        juce::Path spec;
        spec.startNewSubPath (p.getX(), p.getBottom());
        for (int c = 0; c < nCols; ++c)
            spec.lineTo (p.getX() + (float) c, levelToY (colLevel[(size_t) c]));
        spec.lineTo (p.getRight(), p.getBottom());
        spec.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colours::spectrum.withAlpha (0.7f), 0, p.getY(),
                                                 colours::spectrum.withAlpha (0.1f), 0, p.getBottom(), false));
        g.fillPath (spec);

        // reduction (hangs from the top)
        juce::Path red;
        red.startNewSubPath (p.getX(), p.getY());
        float maxRed = 0.0f;
        for (int c = 0; c < nCols; ++c)
        {
            red.lineTo (p.getX() + (float) c, reductionToY (colRed[(size_t) c]));
            maxRed = std::max (maxRed, colRed[(size_t) c]);
        }
        red.lineTo (p.getRight(), p.getY());
        red.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colours::reduction.withAlpha (0.15f), 0, p.getY(),
                                                 colours::reduction.withAlpha (0.55f), 0, reductionToY (30.0f), false));
        g.fillPath (red);
        g.setColour (colours::reduction);
        g.strokePath (red, juce::PathStrokeType (1.5f));

        g.setColour (colours::reduction);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("max -" + juce::String (maxRed, 1) + " dB", p.reduced (8.0f, 4.0f).removeFromTop (18).removeFromRight (120),
                    juce::Justification::centredRight);
        p = plot();
    }

    // sensitivity curve + nodes
    const auto nodes = proc.getNodes();
    juce::Path curve;
    for (int c = 0; c <= (int) p.getWidth(); c += 2)
    {
        const float x = p.getX() + (float) c;
        const float y = weightToY (serene::shapingWeightDb (nodes, xToFreq (x)));
        if (c == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    g.setColour (colours::accent.withAlpha (0.25f));
    g.drawHorizontalLine ((int) weightToY (0.0f), p.getX(), p.getRight());
    g.setColour (colours::accent.withAlpha (0.9f));
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    for (int i = 0; i < serene::numNodes; ++i)
    {
        const juce::Point<float> pt (freqToX (nodes.freq[(size_t) i]), weightToY (nodes.gain[(size_t) i]));
        const bool hot = i == hoverNode || i == dragNode;
        const float r = hot ? 8.0f : 6.0f;
        g.setColour (colours::bg);
        g.fillEllipse (pt.x - r, pt.y - r, r * 2, r * 2);
        g.setColour (hot ? colours::accent : colours::accent.withAlpha (0.8f));
        g.drawEllipse (pt.x - r, pt.y - r, r * 2, r * 2, 2.0f);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (pt.x - r, pt.y - r, r * 2, r * 2), juce::Justification::centred);

        if (hot)
        {
            const float f = nodes.freq[(size_t) i];
            const juce::String txt = (f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz" : juce::String (f, 0) + " Hz")
                                     + "   " + (nodes.gain[(size_t) i] >= 0 ? "+" : "") + juce::String (nodes.gain[(size_t) i], 1) + " dB"
                                     + "   Q " + juce::String (nodes.q[(size_t) i], 2);
            juce::Rectangle<float> box (pt.x - 85, pt.y - 34, 170, 20);
            box = box.constrainedWithin (p);
            g.setColour (colours::bg.withAlpha (0.85f));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (colours::text);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (txt, box, juce::Justification::centred);
        }
    }
}

int SpectrumView::hitNode (juce::Point<float> pos) const
{
    const auto nodes = proc.getNodes();
    int best = -1;
    float bestD = 14.0f;
    for (int i = 0; i < serene::numNodes; ++i)
    {
        const juce::Point<float> pt (freqToX (nodes.freq[(size_t) i]), weightToY (nodes.gain[(size_t) i]));
        const float d = pt.getDistanceFrom (pos);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

juce::RangedAudioParameter* SpectrumView::param (const juce::String& id) const
{
    return proc.apvts.getParameter (id);
}

void SpectrumView::setParam (const juce::String& id, float value)
{
    if (auto* prm = param (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hoverNode) { hoverNode = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void SpectrumView::mouseExit (const juce::MouseEvent&) { hoverNode = -1; repaint(); }

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    dragNode = hitNode (e.position);
    if (dragNode >= 0)
    {
        param (ids::nodeFreq (dragNode))->beginChangeGesture();
        param (ids::nodeGain (dragNode))->beginChangeGesture();
    }
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0) return;
    auto p = plot();
    const float x = juce::jlimit (p.getX(), p.getRight(), e.position.x);
    setParam (ids::nodeFreq (dragNode), juce::jlimit (minHz, maxHz, xToFreq (x)));
    setParam (ids::nodeGain (dragNode), yToWeight (e.position.y));
    repaint();
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    if (dragNode >= 0)
    {
        param (ids::nodeFreq (dragNode))->endChangeGesture();
        param (ids::nodeGain (dragNode))->endChangeGesture();
    }
    dragNode = -1;
}

void SpectrumView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int n = hitNode (e.position);
    if (n < 0) return;
    auto* prm = param (ids::nodeGain (n));
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 (0.0f));
    prm->endChangeGesture();
    repaint();
}

void SpectrumView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int n = hoverNode >= 0 ? hoverNode : hitNode (e.position);
    if (n < 0) return;
    auto* prm = param (ids::nodeQ (n));
    const float q = prm->convertFrom0to1 (prm->getValue());
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 (juce::jlimit (0.2f, 8.0f, q * std::pow (1.15f, w.deltaY * 8.0f))));
    prm->endChangeGesture();
    repaint();
}
