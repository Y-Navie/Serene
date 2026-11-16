#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SpectralEngine.h"

namespace ids
{
inline constexpr const char* depth    = "depth";
inline constexpr const char* detail   = "detail";
inline constexpr const char* mode     = "mode";      // Soft / Hard
inline constexpr const char* attack   = "attack";
inline constexpr const char* release  = "release";
inline constexpr const char* mix      = "mix";
inline constexpr const char* trim     = "trim";
inline constexpr const char* delta    = "delta";
inline constexpr const char* quality  = "quality";   // Low latency / Normal / High
inline constexpr const char* channels = "channels";  // Linked / Unlinked / Mid-Side
inline constexpr const char* bypass   = "bypass";

inline juce::String nodeFreq (int i) { return "n" + juce::String (i) + "_freq"; }
inline juce::String nodeGain (int i) { return "n" + juce::String (i) + "_gain"; }
inline juce::String nodeQ    (int i) { return "n" + juce::String (i) + "_q"; }
}

class SereneProcessor : public juce::AudioProcessor
{
public:
    SereneProcessor();
    ~SereneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Serene"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    serene::NodeSet getNodes() const;

    // GUI side: copies the latest analysis. Returns false if nothing new.
    bool fetchDisplay (std::vector<float>& levelDb, std::vector<float>& reductionDb, double& binHz);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    int orderForQuality (int q) const;
    void switchQuality (int q);

    std::array<serene::SpectralEngine, 3> engines;
    int activeQuality = -1;
    double sr = 44100.0;

    // dry path delay, aligned with engine latency
    std::vector<std::vector<float>> dryDelay;
    int dryWrite = 0, dryDelaySamples = 0;
    juce::AudioBuffer<float> dryBuf;

    juce::SmoothedValue<float> mixSm, trimSm;

    juce::SpinLock displayLock;
    std::vector<float> dispLevel, dispRed;
    double dispBinHz = 1.0;
    bool dispNew = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SereneProcessor)
};
