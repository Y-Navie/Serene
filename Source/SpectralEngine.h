#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Shaping.h"

namespace serene
{
struct DetectionSettings
{
    float depthDb   = 6.0f;   // 0..24
    float detail    = 0.5f;   // 0..1 (0 = broad/smooth, 1 = narrow/precise)
    float attackMs  = 5.0f;
    float releaseMs = 60.0f;
    bool  softMode  = true;
    bool  linked    = true;   // one shared reduction curve for all channels
};

// STFT dynamic resonance suppressor.
// Hann analysis + synthesis window, 75% overlap. Latency = FFT size.
class SpectralEngine
{
public:
    void prepare (int fftOrder, int numChannels, double sampleRate);
    void reset();

    void setSettings (const DetectionSettings& s) { settings = s; }
    void setNodes (const NodeSet& n);

    // Processes in place. Channel count must match prepare().
    void process (float* const* channels, int numChannels, int numSamples);

    int getLatency() const noexcept   { return fftSize; }
    int getNumBins() const noexcept   { return numBins; }
    double getBinHz() const noexcept  { return sampleRate / fftSize; }

    // Most recent analysis (bins), for the display. Audio-thread owned.
    const std::vector<float>& getLastLevelDb() const     { return displayLevel; }
    const std::vector<float>& getLastReductionDb() const { return displayReduction; }

private:
    void processFrame();
    void computeTarget (const float* levelDb, float* target);
    void smoothInTime (const float* target, float* state);
    void updateWidths();
    static void boxBlur (const float* in, float* out, const std::vector<int>& halfWidth,
                         std::vector<double>& prefix, int n);

    struct Channel
    {
        std::vector<float> inFifo, outFifo, spec, levelDb, state, target;
    };

    std::unique_ptr<juce::dsp::FFT> fft;
    int fftSize = 0, hop = 0, numBins = 0, pos = 0, hopCount = 0;
    double sampleRate = 44100.0;
    float olaScale = 1.0f, dbOffset = 0.0f;

    std::vector<Channel> chans;
    std::vector<float> window, binHz, weightScale, edgeScale;
    std::vector<float> det, baseline, tmp, sharedTarget, sharedState;
    std::vector<double> prefix;
    std::vector<int> baseHalfWidth, smoothHalfWidth;
    std::vector<float> displayLevel, displayReduction;

    DetectionSettings settings;
    float lastDetail = -1.0f;
    NodeSet nodes;
    bool nodesValid = false;
};
} // namespace serene
