#include "VocalCleanup.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace blackie
{
void VocalCleanup::prepare(double sampleRate)
{
    model.reset();
    if (sampleRate == 48000.0)
    {
        model.reset(rnnoise_create());
        if (model)
        {
            // RNNoise v0.1 initializes its shared FFT tables on first inference.
            static std::once_flag warmup;
            std::call_once(warmup, [this]
            {
                rnnoise_process_frame(model.get(), cleanFrame.data(), inputFrame.data());
                rnnoise_init(model.get());
            });
        }
    }

    inputFrame.fill(0);
    cleanFrame.fill(0);
    dryDelay.fill(0);
    position = 0;
    low = body = presence = level = peak = 0;
    roomGain = 1;
    current = Settings {};
    const auto pole = [sampleRate](double hertz)
    {
        return static_cast<float>(1.0 - std::exp(-6.28318530718 * hertz / sampleRate));
    };
    lowRate = pole(70);
    bodyRate = pole(320);
    presenceRate = pole(3200);
    attack = pole(80);
    release = pole(12);
    peakRelease = pole(0.45);
    gainRate = pole(16);
    controlRate = pole(25);
}

float VocalCleanup::process(float input, Settings settings) noexcept
{
    current.noise += controlRate * (settings.noise - current.noise);
    current.room += controlRate * (settings.room - current.room);
    current.tone += controlRate * (settings.tone - current.tone);

    float sample = input;
    if (model)
    {
        const int framePosition = position % frameSize;
        const float dry = dryDelay[position];
        dryDelay[position] = input;
        sample = dry + current.noise * (cleanFrame[framePosition] / 32768.0f - dry);
        inputFrame[framePosition] = input * 32768.0f;
        if (framePosition == frameSize - 1)
            rnnoise_process_frame(model.get(), cleanFrame.data(), inputFrame.data());

        position = (position + 1) % static_cast<int>(dryDelay.size());
    }

    // Follow recent syllable level and gently attenuate quieter tails.
    const float magnitude = std::abs(sample);
    level += (magnitude > level ? attack : release) * (magnitude - level);
    peak = std::max(level, peak * (1.0f - peakRelease));
    const float threshold = std::max(0.0005f, peak * 0.18f);
    const float tail = std::clamp(level / threshold, 0.0f, 1.0f);
    roomGain += gainRate * (1.0f - current.room * 0.8f * (1.0f - tail) - roomGain);

    // Broad, low-gain contour for rumble, low-mid buildup, and articulation.
    low += lowRate * (sample - low);
    body += bodyRate * (sample - body);
    presence += presenceRate * (sample - presence);
    const float contour = -0.9f * low - 0.18f * (body - low) + 0.12f * (sample - presence);
    return (sample + current.tone * contour) * roomGain;
}
}
