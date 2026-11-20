#pragma once

#include <array>
#include <memory>
extern "C"
{
#include <rnnoise.h>
}

namespace blackie
{
struct Settings
{
    float noise = 0.65f;
    float room = 0.35f;
    float tone = 0.40f;
};

class VocalCleanup
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    int latency() const noexcept { return model ? 2 * frameSize : 0; }
    float process(float input, Settings settings, bool bypassed = false) noexcept;

private:
    static constexpr int frameSize = 480;
    using Model = std::unique_ptr<DenoiseState, decltype(&rnnoise_destroy)>;
    Model model { nullptr, rnnoise_destroy };
    std::array<float, frameSize> inputFrame {}, cleanFrame {};
    std::array<float, 2 * frameSize> dryDelay {};
    int position = 0;
    float low = 0, body = 0, presence = 0;
    float level = 0, peak = 0, roomGain = 1;
    float lowRate = 0, bodyRate = 0, presenceRate = 0;
    float attack = 0, release = 0, peakRelease = 0, gainRate = 0, controlRate = 0;
    Settings current;
};
}
