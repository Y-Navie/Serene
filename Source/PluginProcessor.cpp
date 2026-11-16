#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const float defaultNodeFreq[serene::numNodes] = { 80.0f, 250.0f, 800.0f, 2500.0f, 7000.0f, 12000.0f };
const float defaultNodeQ[serene::numNodes]    = { 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f };
}

SereneProcessor::SereneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SereneProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto db  = [] (float v, int) { return String (v, 1) + " dB"; };
    auto ms  = [] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; };
    auto pct = [] (float v, int) { return String (v, 0) + " %"; };

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::depth, 1 }, "Depth",
            NormalisableRange<float> (0.0f, 24.0f, 0.01f, 0.7f), 6.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::detail, 1 }, "Detail",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::mode, 1 }, "Mode", StringArray { "Soft", "Hard" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::attack, 1 }, "Attack",
            NormalisableRange<float> (0.5f, 100.0f, 0.01f, 0.4f), 5.0f, AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::release, 1 }, "Release",
            NormalisableRange<float> (5.0f, 1000.0f, 0.1f, 0.4f), 60.0f, AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, 1 }, "Mix",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::trim, 1 }, "Trim",
            NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::delta, 1 }, "Delta", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::quality, 1 }, "Quality",
            StringArray { "Low latency", "Normal", "High" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::channels, 1 }, "Channels",
            StringArray { "Linked", "Unlinked", "Mid/Side" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));

    for (int i = 0; i < serene::numNodes; ++i)
    {
        const String n = i == 0 ? "Low shelf" : (i == serene::numNodes - 1 ? "High shelf" : "Band " + String (i));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::nodeFreq (i), 1 }, n + " Freq",
                NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.25f), defaultNodeFreq[i],
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
                    return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (v, 0) + " Hz"; })));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::nodeGain (i), 1 }, n + " Sensitivity",
                NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::nodeQ (i), 1 }, n + " Q",
                NormalisableRange<float> (0.2f, 8.0f, 0.01f, 0.5f), defaultNodeQ[i]));
    }
    return l;
}

bool SereneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

int SereneProcessor::orderForQuality (int q) const
{
    int order = q == 0 ? 8 : (q == 1 ? 11 : 12);  // 256 / 2048 / 4096 at 44.1-48 kHz
    if (sr > 50000.0)  ++order;                   // keep the same time/frequency resolution
    if (sr > 100000.0) ++order;
    return order;
}

void SereneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int nCh = std::max (1, getTotalNumOutputChannels());

    int maxLatency = 0;
    for (int q = 0; q < 3; ++q)
    {
        engines[(size_t) q].prepare (orderForQuality (q), nCh, sr);
        maxLatency = std::max (maxLatency, engines[(size_t) q].getLatency());
    }

    dryDelay.assign ((size_t) nCh, std::vector<float> ((size_t) maxLatency + 1, 0.0f));
    dryWrite = 0;
    dryBuf.setSize (nCh, std::max (1, samplesPerBlock));

    mixSm.reset (sr, 0.05);
    trimSm.reset (sr, 0.05);
    mixSm.setCurrentAndTargetValue (apvts.getRawParameterValue (ids::mix)->load() * 0.01f);
    trimSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ids::trim)->load()));

    activeQuality = -1;
    switchQuality ((int) apvts.getRawParameterValue (ids::quality)->load());
}

void SereneProcessor::switchQuality (int q)
{
    q = juce::jlimit (0, 2, q);
    if (q == activeQuality)
        return;

    activeQuality = q;
    engines[(size_t) q].reset();
    dryDelaySamples = engines[(size_t) q].getLatency();
    for (auto& d : dryDelay)
        std::fill (d.begin(), d.end(), 0.0f);
    setLatencySamples (dryDelaySamples);
}

serene::NodeSet SereneProcessor::getNodes() const
{
    serene::NodeSet n;
    for (int i = 0; i < serene::numNodes; ++i)
    {
        n.freq[(size_t) i] = apvts.getRawParameterValue (ids::nodeFreq (i))->load();
        n.gain[(size_t) i] = apvts.getRawParameterValue (ids::nodeGain (i))->load();
        n.q[(size_t) i]    = apvts.getRawParameterValue (ids::nodeQ (i))->load();
    }
    return n;
}

void SereneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = std::min (buffer.getNumChannels(), (int) dryDelay.size());
    const int n   = buffer.getNumSamples();
    for (int c = nCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    if (nCh == 0 || n == 0)
        return;

    switchQuality ((int) apvts.getRawParameterValue (ids::quality)->load());
    auto& engine = engines[(size_t) activeQuality];

    const int chMode = (int) apvts.getRawParameterValue (ids::channels)->load();
    const bool ms    = chMode == 2 && nCh == 2;

    serene::DetectionSettings s;
    s.depthDb   = apvts.getRawParameterValue (ids::depth)->load();
    s.detail    = apvts.getRawParameterValue (ids::detail)->load() * 0.01f;
    s.softMode  = apvts.getRawParameterValue (ids::mode)->load() < 0.5f;
    s.attackMs  = apvts.getRawParameterValue (ids::attack)->load();
    s.releaseMs = apvts.getRawParameterValue (ids::release)->load();
    s.linked    = chMode == 0;
    engine.setSettings (s);
    engine.setNodes (getNodes());

    const bool delta  = apvts.getRawParameterValue (ids::delta)->load() > 0.5f;
    const bool bypass = apvts.getRawParameterValue (ids::bypass)->load() > 0.5f;
    mixSm.setTargetValue (apvts.getRawParameterValue (ids::mix)->load() * 0.01f);
    trimSm.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ids::trim)->load()));

    // Dry path, delayed to line up with the wet path
    if (dryBuf.getNumSamples() < n)
        dryBuf.setSize (dryBuf.getNumChannels(), n, false, false, true);

    const int dlen = (int) dryDelay[0].size();
    for (int c = 0; c < nCh; ++c)
    {
        auto* in = buffer.getReadPointer (c);
        auto* d  = dryBuf.getWritePointer (c);
        auto& line = dryDelay[(size_t) c];
        int w = dryWrite;
        for (int i = 0; i < n; ++i)
        {
            line[(size_t) w] = in[i];
            int r = w - dryDelaySamples;
            if (r < 0) r += dlen;
            d[i] = line[(size_t) r];
            if (++w >= dlen) w = 0;
        }
    }
    dryWrite = (dryWrite + n) % dlen;

    // Wet path
    if (ms)
    {
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]);
            L[i] = m; R[i] = sd;
        }
    }

    engine.process (buffer.getArrayOfWritePointers(), nCh, n);

    if (ms)
    {
        auto* M = buffer.getWritePointer (0);
        auto* S = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float l = M[i] + S[i], r = M[i] - S[i];
            M[i] = l; S[i] = r;
        }
    }

    // Output stage
    for (int i = 0; i < n; ++i)
    {
        const float mix = mixSm.getNextValue();
        const float g   = trimSm.getNextValue();
        for (int c = 0; c < nCh; ++c)
        {
            auto* o = buffer.getWritePointer (c);
            const float dry = dryBuf.getSample (c, i);
            float y;
            if (bypass)      y = dry;
            else if (delta)  y = (dry - o[i]) * g;           // what is being removed
            else             y = (o[i] * mix + dry * (1.0f - mix)) * g;
            o[i] = y;
        }
    }

    // Hand the analysis to the GUI (never blocks the audio thread)
    {
        const juce::SpinLock::ScopedTryLockType lock (displayLock);
        if (lock.isLocked())
        {
            dispLevel = engine.getLastLevelDb();     // same size after first copy -> no realloc
            dispRed   = engine.getLastReductionDb();
            dispBinHz = engine.getBinHz();
            dispNew   = true;
        }
    }
}

bool SereneProcessor::fetchDisplay (std::vector<float>& levelDb, std::vector<float>& reductionDb, double& binHz)
{
    const juce::SpinLock::ScopedLockType lock (displayLock);
    if (! dispNew)
        return false;
    levelDb = dispLevel;
    reductionDb = dispRed;
    binHz = dispBinHz;
    dispNew = false;
    return true;
}

juce::AudioProcessorEditor* SereneProcessor::createEditor() { return new SereneEditor (*this); }

void SereneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SereneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SereneProcessor(); }
