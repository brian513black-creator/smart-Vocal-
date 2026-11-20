#include "ReferenceEq.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace blackie
{
float ReferenceEq::Bandpass::process(float input, const Coefficients& coefficients) noexcept
{
    const float feedforward = coefficients.b0 * input;
    const float output = feedforward + z1;
    z1 = z2 - coefficients.a1 * output;
    z2 = -feedforward - coefficients.a2 * output;
    return output;
}

void ReferenceEq::prepare(double sampleRate) noexcept
{
    reset();
    windowSize = 0;
    if (sampleRate < 16000.0)
        return;

    constexpr std::array<double, bandCount> centreHz { 240, 760, 2200, 5800 };
    constexpr double q = 0.85;
    for (std::size_t band = 0; band < bandCount; ++band)
    {
        const double omega = 6.28318530718 * centreHz[band] / sampleRate;
        const double alpha = std::sin(omega) / (2 * q);
        coefficients[band] = {
            static_cast<float>(alpha / (1 + alpha)),
            static_cast<float>(-2 * std::cos(omega) / (1 + alpha)),
            static_cast<float>((1 - alpha) / (1 + alpha))
        };
    }

    windowSize = static_cast<int>(std::lround(0.1 * sampleRate));
    smoothing = static_cast<float>(-std::expm1(-1.0 / (0.12 * sampleRate)));
}

void ReferenceEq::reset() noexcept
{
    filters = {};
    windowPower.fill(0);
    averageShare.fill(0);
    correction.fill(0);
    appliedCorrection.fill(0);
    windowInputPower = recentPeakPower = 0;
    samplesInWindow = activeWindows = 0;
    running = false;
}

void ReferenceEq::process(float* const* channels, int numChannels, int numSamples, float amount) noexcept
{
    if (windowSize == 0 || (!running && amount == 0))
        return;

    running = true;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (std::size_t band = 0; band < bandCount; ++band)
            appliedCorrection[band] += smoothing * (amount * correction[band] - appliedCorrection[band]);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float input = channels[channel][sample];
            windowInputPower += static_cast<double>(input) * input;
            float output = input;
            for (std::size_t band = 0; band < bandCount; ++band)
            {
                const float filtered = filters[channel][band].process(input, coefficients[band]);
                windowPower[band] += static_cast<double>(filtered) * filtered;
                output += appliedCorrection[band] * filtered;
            }
            channels[channel][sample] = output;
        }

        if (++samplesInWindow == windowSize)
            finishWindow(numChannels);
    }

    if (amount == 0 && std::all_of(appliedCorrection.begin(), appliedCorrection.end(), [](float gain)
        { return std::abs(gain) < 0.0001f; }))
        reset();
}

void ReferenceEq::finishWindow(int numChannels) noexcept
{
    constexpr double minimumMeanPower = 1e-7; // -70 dBFS
    constexpr double relativeGate = 0.004;   // About 24 dB below recent peaks
    constexpr int warmupWindows = 5;
    const double meanPower = windowInputPower / (windowSize * numChannels);
    recentPeakPower = std::max(meanPower, recentPeakPower * 0.96);
    const double total = std::accumulate(windowPower.begin(), windowPower.end(), 0.0);
    if (meanPower >= std::max(minimumMeanPower, recentPeakPower * relativeGate) && total > 0)
    {
        // Give active windows equal weight regardless of their level.
        const double weight = activeWindows < warmupWindows ? 1.0 / (activeWindows + 1) : 0.05;
        for (std::size_t band = 0; band < bandCount; ++band)
            averageShare[band] += weight * (windowPower[band] / total - averageShare[band]);

        activeWindows = std::min(activeWindows + 1, warmupWindows);
        if (activeWindows == warmupWindows)
        {
            // brian.mp3, 44.1 kHz, powers summed across channels and 100 ms active windows.
            constexpr std::array<double, bandCount> referenceDb { -7.755, -4.163, -5.002, -8.767 };
            std::array<double, bandCount> difference {};
            for (std::size_t band = 0; band < bandCount; ++band)
            {
                const double sourceDb = 10 * std::log10(std::max(averageShare[band], 1e-5));
                difference[band] = referenceDb[band] - sourceDb;
            }

            const double mean = std::accumulate(difference.begin(), difference.end(), 0.0) / bandCount;
            constexpr std::array<double, bandCount> maxBoostDb { 2.0, 2.0, 2.0, 1.25 };
            for (std::size_t band = 0; band < bandCount; ++band)
            {
                const double error = difference[band] - mean;
                const double outsideDeadband = std::copysign(std::max(0.0, std::abs(error) - 0.4), error);
                // Avoid boosting bands with little signal, particularly near sibilance.
                const double confidence = std::min(1.0, averageShare[band] / 0.01);
                const double decibels = std::clamp(outsideDeadband, -3.0, maxBoostDb[band] * confidence);
                correction[band] = static_cast<float>(std::pow(10.0, decibels / 20.0) - 1.0);
            }
        }
    }

    windowPower.fill(0);
    windowInputPower = 0;
    samplesInWindow = 0;
}
}
