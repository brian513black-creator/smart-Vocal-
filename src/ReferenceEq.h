#pragma once

#include <array>
#include <cstddef>

namespace blackie
{
class ReferenceEq
{
public:
    static constexpr float defaultAmount = 0.8f;

    void prepare(double sampleRate) noexcept;
    void process(float* const* channels, int numChannels, int numSamples, float amount) noexcept;

private:
    static constexpr std::size_t bandCount = 4;

    struct Coefficients
    {
        float b0 = 0, a1 = 0, a2 = 0;
    };

    struct Bandpass
    {
        float process(float input, const Coefficients& coefficients) noexcept;
        float z1 = 0, z2 = 0;
    };

    void reset() noexcept;
    void finishWindow(int numChannels) noexcept;

    std::array<Coefficients, bandCount> coefficients {};
    std::array<std::array<Bandpass, bandCount>, 2> filters {};
    std::array<double, bandCount> windowPower {}, averageShare {};
    std::array<float, bandCount> correction {}, appliedCorrection {};
    double windowInputPower = 0, recentPeakPower = 0;
    float smoothing = 0;
    int windowSize = 0, samplesInWindow = 0, activeWindows = 0;
    bool running = false;
};
}
