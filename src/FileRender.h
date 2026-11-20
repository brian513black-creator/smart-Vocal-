#pragma once

#include "VocalCleanup.h"
#include "ReferenceEq.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>

namespace blackie
{
struct RenderSettings
{
    Settings cleanup;
    float autoEq = ReferenceEq::defaultAmount;
};

juce::Result renderVocalFile(const juce::File& input, const juce::File& output,
                            RenderSettings settings, const std::atomic<bool>& cancel,
                            std::atomic<double>& progress);
}
