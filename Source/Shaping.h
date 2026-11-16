#pragma once
#include <array>
#include <cmath>

// Six sensitivity-shaping nodes: 0 = low shelf, 1..4 = bells, 5 = high shelf.
// The curve is a sensitivity weight in "dB": 0 = normal, -12 = no processing,
// +12 = double the reduction. Shared by the DSP and the GUI so they always match.
namespace serene
{
constexpr int numNodes = 6;

struct NodeSet
{
    std::array<float, numNodes> freq {}, gain {}, q {};

    bool operator== (const NodeSet& o) const { return freq == o.freq && gain == o.gain && q == o.q; }
    bool operator!= (const NodeSet& o) const { return ! (*this == o); }
};

inline float sigmoid (float x) { return 1.0f / (1.0f + std::exp (-x)); }

inline float shapingWeightDb (const NodeSet& n, float hz)
{
    const float lf = std::log2 (std::max (hz, 1.0f));
    float sum = 0.0f;

    for (int i = 0; i < numNodes; ++i)
    {
        if (std::abs (n.gain[(size_t) i]) < 1.0e-6f)
            continue;

        const float d = lf - std::log2 (n.freq[(size_t) i]);
        const float q = n.q[(size_t) i];

        if (i == 0)                       // low shelf
            sum += n.gain[0] * (1.0f - sigmoid (d * q * 6.0f));
        else if (i == numNodes - 1)       // high shelf
            sum += n.gain[(size_t) i] * sigmoid (d * q * 6.0f);
        else                              // bell, width ~ 1/Q octaves
        {
            const float sigma = 0.5f / q;
            sum += n.gain[(size_t) i] * std::exp (-0.5f * (d / sigma) * (d / sigma));
        }
    }
    return sum;
}

// Converts the weight to a multiplier applied to the reduction amount.
inline float weightToScale (float weightDb)
{
    const float s = 1.0f + weightDb / 12.0f;
    return s < 0.0f ? 0.0f : (s > 2.0f ? 2.0f : s);
}
} // namespace serene
