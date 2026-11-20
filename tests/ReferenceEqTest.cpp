#include "ReferenceEq.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    constexpr int sampleRate = 44100;
    constexpr int sampleCount = 4 * sampleRate;
    const auto tone = [](int sample, int frequency)
    {
        return std::sin(6.28318530718 * frequency * sample / sampleRate);
    };

    std::vector<float> left(sampleCount), right(sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        left[sample] = static_cast<float>(0.03 * tone(sample, 240) + 0.04 * tone(sample, 760)
                                         + 0.04 * tone(sample, 2200) + 0.12 * tone(sample, 5800));
        right[sample] = -0.5f * left[sample];
    }

    blackie::ReferenceEq eq;
    eq.prepare(sampleRate);
    const auto original = left;
    float* initialChannels[] { left.data(), right.data() };
    eq.process(initialChannels, 2, sampleCount, 0.0f);
    if (left != original)
    {
        std::cerr << "Match Brian should pass audio through at zero amount\n";
        return 1;
    }

    for (int offset = 0; offset < sampleCount; offset += 257)
    {
        float* channels[] { left.data() + offset, right.data() + offset };
        eq.process(channels, 2, std::min(257, sampleCount - offset), blackie::ReferenceEq::defaultAmount);
    }

    double mid = 0, high = 0;
    for (int sample = sampleCount - sampleRate; sample < sampleCount; ++sample)
    {
        if (!std::isfinite(left[sample]) || std::abs(right[sample] + 0.5f * left[sample]) > 1e-6f)
        {
            std::cerr << "Match Brian should apply the same correction to both channels\n";
            return 1;
        }

        mid += left[sample] * tone(sample, 760);
        high += left[sample] * tone(sample, 5800);
    }

    if (mid < 0.015 * sampleRate || high > mid * 2.4)
    {
        std::cerr << "Match Brian did not reduce excess highs relative to the mids\n";
        return 1;
    }

    eq.process(initialChannels, 2, sampleCount, 0.0f);
    float nextLeft = 0.5f, nextRight = -0.25f;
    float* nextChannels[] { &nextLeft, &nextRight };
    eq.process(nextChannels, 2, 1, 0.0f);
    if (nextLeft != 0.5f || nextRight != -0.25f)
    {
        std::cerr << "Match Brian should return to passthrough after fading out\n";
        return 1;
    }
}
