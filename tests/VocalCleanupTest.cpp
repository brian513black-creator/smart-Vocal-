#include "VocalCleanup.h"

#include <cmath>
#include <cstdint>
#include <iostream>

int main()
{
    constexpr int latency = 960;
    blackie::VocalCleanup cleanup;
    cleanup.prepare(48000);
    if (cleanup.latency() != latency)
    {
        std::cerr << "Unexpected 48 kHz latency\n";
        return 1;
    }

    const blackie::Settings dry { 0, 0, 0 };
    for (int sample = 0; sample <= 10000 + latency; ++sample)
    {
        const float output = cleanup.process(sample == 10000 ? 0.5f : 0.0f, dry);
        const float expected = sample == 10000 + latency ? 0.5f : 0.0f;
        if (!std::isfinite(output) || std::abs(output - expected) > 0.00001f)
        {
            std::cerr << "Dry signal did not match reported latency\n";
            return 1;
        }
    }

    cleanup.reset();
    cleanup.process(0.5f, {});
    for (int sample = 1; sample <= latency; ++sample)
    {
        const float expected = sample == latency ? 0.5f : 0.0f;
        if (cleanup.process(0, {}, true) != expected)
        {
            std::cerr << "Bypass did not preserve the delayed dry signal\n";
            return 1;
        }
    }

    cleanup.process(0.5f, {});
    cleanup.reset();
    for (int sample = 0; sample <= latency; ++sample)
    {
        if (cleanup.process(0, {}) != 0)
        {
            std::cerr << "Reset left audio in the delay or denoiser\n";
            return 1;
        }
    }

    cleanup.prepare(48000);
    std::uint32_t generator = 1;
    double inputEnergy = 0;
    double outputEnergy = 0;
    float input = 0;
    for (int sample = 0; sample < 96000; ++sample)
    {
        generator = 1664525u * generator + 1013904223u;
        const float white = static_cast<float>(generator >> 16) / 32768.0f - 1.0f;
        input += 0.18f * (0.08f * white - input);
        const float output = cleanup.process(input, { 1, 0, 0 });
        if (!std::isfinite(output))
            return 1;
        if (sample >= 48000)
        {
            inputEnergy += input * input;
            outputEnergy += output * output;
        }
    }
    if (outputEnergy >= inputEnergy * 0.5)
    {
        std::cerr << "RNNoise did not sufficiently attenuate the stationary noise fixture: " << outputEnergy / inputEnergy << "\n";
        return 1;
    }

    cleanup.prepare(44100);
    if (cleanup.latency() != 0)
        return 1;

    float output = 0;
    for (int sample = 0; sample < 44100; ++sample)
        output = cleanup.process(0.25f, { 1, 0, 1 });

    if (!std::isfinite(output) || output < 0.01f || output > 0.05f)
    {
        std::cerr << "Natural EQ did not reduce low-frequency buildup at 44.1 kHz\n";
        return 1;
    }

    blackie::VocalCleanup untreated;
    untreated.prepare(44100);
    cleanup.prepare(44100);
    double untreatedTail = 0;
    double treatedTail = 0;
    for (int sample = 0; sample < 30000; ++sample)
    {
        const double time = sample / 44100.0;
        const double amplitude = time < 0.2 ? 0.5 : 0.05 * std::exp(-6 * (time - 0.2));
        const float signal = static_cast<float>(amplitude * std::sin(6.28318530718 * 220 * time));
        const float clean = cleanup.process(signal, { 0, 1, 0 });
        const float original = untreated.process(signal, dry);
        if (!std::isfinite(clean))
            return 1;
        if (time >= 0.28 && time <= 0.5)
        {
            treatedTail += clean * clean;
            untreatedTail += original * original;
        }
    }
    if (treatedTail >= untreatedTail * 0.8)
    {
        std::cerr << "Room Tail did not attenuate the simulated decaying tail\n";
        return 1;
    }
}
