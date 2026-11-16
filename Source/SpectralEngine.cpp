#include "SpectralEngine.h"

namespace serene
{
void SpectralEngine::prepare (int fftOrder, int numChannels, double sr)
{
    fftSize    = 1 << fftOrder;
    hop        = fftSize / 4;
    numBins    = fftSize / 2 + 1;
    sampleRate = sr;
    fft        = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);

    // Hann^2 summed at 75% overlap = 1.5
    olaScale = 1.0f / 1.5f;
    // A full-scale sine peaks at N/4 with a Hann window -> normalise to ~dBFS
    dbOffset = -20.0f * std::log10 ((float) fftSize / 4.0f);

    chans.assign ((size_t) numChannels, {});
    for (auto& c : chans)
    {
        c.inFifo.assign ((size_t) fftSize, 0.0f);
        c.outFifo.assign ((size_t) fftSize, 0.0f);
        c.spec.assign ((size_t) fftSize * 2, 0.0f);
        c.levelDb.assign ((size_t) numBins, -120.0f);
        c.state.assign ((size_t) numBins, 0.0f);
        c.target.assign ((size_t) numBins, 0.0f);
    }

    const auto nb = (size_t) numBins;
    binHz.resize (nb);
    edgeScale.resize (nb);
    weightScale.assign (nb, 1.0f);
    det.assign (nb, -120.0f);
    baseline.assign (nb, 0.0f);
    tmp.assign (nb, 0.0f);
    sharedTarget.assign (nb, 0.0f);
    sharedState.assign (nb, 0.0f);
    prefix.assign (nb + 1, 0.0);
    baseHalfWidth.assign (nb, 1);
    smoothHalfWidth.assign (nb, 0);
    displayLevel.assign (nb, -120.0f);
    displayReduction.assign (nb, 0.0f);

    for (size_t k = 0; k < nb; ++k)
    {
        const float hz = (float) k * (float) (sampleRate / fftSize);
        binHz[k] = hz;
        // fade processing in above 20 Hz and out towards 20 kHz / Nyquist
        const float lo = juce::jlimit (0.0f, 1.0f, (hz - 15.0f) / 15.0f);
        const float top = (float) std::min (20000.0, sampleRate * 0.45);
        const float hi = juce::jlimit (0.0f, 1.0f, (top - hz) / (top * 0.1f));
        edgeScale[k] = lo * hi;
    }

    lastDetail = -1.0f;
    nodesValid = false;
    setNodes (nodes);
    reset();
}

void SpectralEngine::reset()
{
    pos = 0;
    hopCount = 0;
    for (auto& c : chans)
    {
        std::fill (c.inFifo.begin(), c.inFifo.end(), 0.0f);
        std::fill (c.outFifo.begin(), c.outFifo.end(), 0.0f);
        std::fill (c.state.begin(), c.state.end(), 0.0f);
    }
    std::fill (sharedState.begin(), sharedState.end(), 0.0f);
    std::fill (displayReduction.begin(), displayReduction.end(), 0.0f);
    std::fill (displayLevel.begin(), displayLevel.end(), -120.0f);
}

void SpectralEngine::setNodes (const NodeSet& n)
{
    if (nodesValid && n == nodes)
        return;

    nodes = n;
    nodesValid = true;
    for (size_t k = 0; k < binHz.size(); ++k)
        weightScale[k] = weightToScale (shapingWeightDb (nodes, std::max (binHz[k], 1.0f)));
}

void SpectralEngine::updateWidths()
{
    if (std::abs (settings.detail - lastDetail) < 1.0e-4f)
        return;
    lastDetail = settings.detail;

    auto lerpLog = [] (float a, float b, float t) { return std::exp2 (std::log2 (a) + (std::log2 (b) - std::log2 (a)) * t); };

    // Baseline (what counts as "normal" spectrum): wide at low detail, narrow at high detail
    const float baseOct   = lerpLog (2.0f, 1.0f / 6.0f, settings.detail);
    // Smoothing of the reduction curve itself
    const float smoothOct = lerpLog (1.0f / 3.0f, 1.0f / 30.0f, settings.detail);

    auto factor = [] (float oct) { return (std::exp2 (oct * 0.5f) - std::exp2 (-oct * 0.5f)) * 0.5f; };
    const float cb = factor (baseOct), cs = factor (smoothOct);
    const int maxW = numBins / 4;

    for (int k = 0; k < numBins; ++k)
    {
        baseHalfWidth[(size_t) k]   = juce::jlimit (2, maxW, (int) std::lround ((float) k * cb));
        smoothHalfWidth[(size_t) k] = juce::jlimit (0, maxW, (int) std::lround ((float) k * cs));
    }
}

void SpectralEngine::boxBlur (const float* in, float* out, const std::vector<int>& hw,
                              std::vector<double>& p, int n)
{
    p[0] = 0.0;
    for (int k = 0; k < n; ++k)
        p[(size_t) k + 1] = p[(size_t) k] + in[k];

    for (int k = 0; k < n; ++k)
    {
        const int lo = std::max (0, k - hw[(size_t) k]);
        const int hi = std::min (n - 1, k + hw[(size_t) k]);
        out[k] = (float) ((p[(size_t) hi + 1] - p[(size_t) lo]) / (double) (hi - lo + 1));
    }
}

void SpectralEngine::computeTarget (const float* level, float* target)
{
    updateWidths();

    // Robust baseline: blur, clip peaks that poke above it, blur again.
    // Stops a strong resonance from lifting its own reference level.
    boxBlur (level, tmp.data(), baseHalfWidth, prefix, numBins);
    for (int k = 0; k < numBins; ++k)
        tmp[(size_t) k] = std::min (level[k], tmp[(size_t) k] + 3.0f);
    boxBlur (tmp.data(), baseline.data(), baseHalfWidth, prefix, numBins);
    for (int k = 0; k < numBins; ++k)
        tmp[(size_t) k] = std::min (level[k], baseline[(size_t) k] + 3.0f);
    boxBlur (tmp.data(), baseline.data(), baseHalfWidth, prefix, numBins);

    float frameMax = -200.0f;
    for (int k = 1; k < numBins; ++k)
        if (edgeScale[(size_t) k] > 0.0f)
            frameMax = std::max (frameMax, level[k]);

    const float floorDb   = std::max (frameMax - 60.0f, -110.0f);
    const float depth     = settings.depthDb;
    const float depthNorm = depth / 12.0f;
    const float maxRed    = depth * 1.5f;

    for (int k = 0; k < numBins; ++k)
    {
        const float excess = level[k] - baseline[(size_t) k];
        float raw, gate;

        if (settings.softMode)
        {
            // soft knee, gentle ratio, wide level gate -> transparent
            const float e = std::max (0.0f, excess - 3.0f);
            raw  = depthNorm * e * e / (e + 6.0f);
            gate = juce::jlimit (0.0f, 1.0f, (level[k] - floorDb) / 30.0f);
        }
        else
        {
            // hard: low threshold, linear, tight gate -> aggressive
            const float e = std::max (0.0f, excess - 1.5f);
            raw  = depthNorm * e;
            gate = juce::jlimit (0.0f, 1.0f, (level[k] - floorDb) / 10.0f);
        }

        raw *= gate * weightScale[(size_t) k] * edgeScale[(size_t) k];
        tmp[(size_t) k] = std::min (raw, maxRed);
    }

    // Widen each notch slightly (max filter) then smooth, so peaks keep their depth
    for (int k = 0; k < numBins; ++k)
    {
        const int w = std::min (smoothHalfWidth[(size_t) k], 24);
        const int lo = std::max (0, k - w), hi = std::min (numBins - 1, k + w);
        float m = 0.0f;
        for (int j = lo; j <= hi; ++j) m = std::max (m, tmp[(size_t) j]);
        baseline[(size_t) k] = m;   // reuse buffer as scratch
    }
    boxBlur (baseline.data(), target, smoothHalfWidth, prefix, numBins);
}

void SpectralEngine::smoothInTime (const float* target, float* state)
{
    const double hopSec = (double) hop / sampleRate;
    const float aC = (float) std::exp (-hopSec / std::max (0.0005, settings.attackMs  * 0.001));
    const float rC = (float) std::exp (-hopSec / std::max (0.001,  settings.releaseMs * 0.001));

    for (int k = 0; k < numBins; ++k)
    {
        const float t = target[k];
        float& s = state[k];
        s = t + (t > s ? aC : rC) * (s - t);
    }
}

void SpectralEngine::processFrame()
{
    const int nCh = (int) chans.size();

    // 1. window + forward FFT + level in dB
    for (auto& c : chans)
    {
        for (int i = 0; i < fftSize; ++i)
            c.spec[(size_t) i] = c.inFifo[(size_t) ((pos + i) % fftSize)] * window[(size_t) i];
        std::fill (c.spec.begin() + fftSize, c.spec.end(), 0.0f);

        fft->performRealOnlyForwardTransform (c.spec.data(), true);

        for (int k = 0; k < numBins; ++k)
        {
            const float re = c.spec[(size_t) (2 * k)], im = c.spec[(size_t) (2 * k + 1)];
            c.levelDb[(size_t) k] = 10.0f * std::log10 (re * re + im * im + 1.0e-20f) + dbOffset;
        }
    }

    // 2. detection
    if (settings.linked || nCh == 1)
    {
        for (int k = 0; k < numBins; ++k)
        {
            float m = -200.0f;
            for (auto& c : chans) m = std::max (m, c.levelDb[(size_t) k]);
            det[(size_t) k] = m;
        }
        computeTarget (det.data(), sharedTarget.data());
        smoothInTime (sharedTarget.data(), sharedState.data());
        for (auto& c : chans)
            std::copy (sharedState.begin(), sharedState.end(), c.state.begin());

        std::copy (det.begin(), det.end(), displayLevel.begin());
        std::copy (sharedState.begin(), sharedState.end(), displayReduction.begin());
    }
    else
    {
        for (auto& c : chans)
        {
            computeTarget (c.levelDb.data(), c.target.data());
            smoothInTime (c.target.data(), c.state.data());
        }
        for (int k = 0; k < numBins; ++k)
        {
            float l = -200.0f, r = 0.0f;
            for (auto& c : chans)
            {
                l = std::max (l, c.levelDb[(size_t) k]);
                r = std::max (r, c.state[(size_t) k]);
            }
            displayLevel[(size_t) k] = l;
            displayReduction[(size_t) k] = r;
        }
    }

    // 3. apply gains, inverse FFT, overlap-add
    for (auto& c : chans)
    {
        auto* s = c.spec.data();
        for (int k = 0; k < numBins; ++k)
        {
            const float g = juce::Decibels::decibelsToGain (-c.state[(size_t) k], -200.0f);
            s[2 * k] *= g;
            s[2 * k + 1] *= g;
        }
        // mirror to a full conjugate-symmetric spectrum (safe for every FFT backend)
        for (int k = numBins; k < fftSize; ++k)
        {
            s[2 * k]     =  s[2 * (fftSize - k)];
            s[2 * k + 1] = -s[2 * (fftSize - k) + 1];
        }

        fft->performRealOnlyInverseTransform (s);

        for (int i = 0; i < fftSize; ++i)
            c.outFifo[(size_t) ((pos + i) % fftSize)] += s[i] * window[(size_t) i] * olaScale;
    }
}

void SpectralEngine::process (float* const* channels, int numChannels, int numSamples)
{
    const int nCh = std::min (numChannels, (int) chans.size());

    for (int n = 0; n < numSamples; ++n)
    {
        for (int c = 0; c < nCh; ++c)
        {
            auto& ch = chans[(size_t) c];
            ch.inFifo[(size_t) pos] = channels[c][n];
            channels[c][n] = ch.outFifo[(size_t) pos];
            ch.outFifo[(size_t) pos] = 0.0f;
        }

        pos = (pos + 1) % fftSize;

        if (++hopCount >= hop)
        {
            hopCount = 0;
            processFrame();
        }
    }
}
} // namespace serene
