#include "FileRender.h"

#include <iostream>

namespace
{
bool checkExport(double sourceRate)
{
    const bool resampled = sourceRate != 48000;
    const int channels = resampled ? 2 : 1;
    const int frames = static_cast<int>(sourceRate / 4);
    const juce::TemporaryFile input(".wav"), output(".wav");
    juce::WavAudioFormat wav;
    auto stream = input.getFile().createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), sourceRate, channels, 32, {}, 0));
    if (!writer)
        return false;
    stream.release();

    juce::AudioBuffer<float> samples(channels, frames);
    samples.clear();
    samples.setSample(0, 0, 0.5f);
    samples.setSample(0, frames - 1, 0.25f);
    if (resampled)
        samples.setSample(1, frames / 2, 0.5f);
    if (!writer->writeFromAudioSampleBuffer(samples, 0, frames))
        return false;
    writer.reset();

    std::atomic<bool> cancel { false };
    std::atomic<double> progress { 0 };
    const blackie::RenderSettings dry { { 0, 0, 0 }, 0 };
    const auto result = blackie::renderVocalFile(input.getFile(), output.getFile(), dry, cancel, progress);
    if (result.failed())
    {
        std::cerr << result.getErrorMessage() << '\n';
        return false;
    }

    const std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(output.getFile().createInputStream().release(), true));
    if (!reader || reader->sampleRate != 48000 || reader->lengthInSamples != 12000
        || reader->numChannels != static_cast<unsigned int>(channels) || progress.load() != 1)
        return false;

    juce::AudioBuffer<float> exported(channels, 12000);
    if (!reader->read(&exported, 0, exported.getNumSamples(), 0, true, true))
        return false;

    if (!resampled)
    {
        for (int sample = 0; sample < frames; ++sample)
            if (exported.getSample(0, sample) != samples.getSample(0, sample))
                return false;
    }
    else if (exported.getMagnitude(1, 5990, 20) < 0.1f || exported.getMagnitude(0, 5990, 20) != 0)
    {
        return false;
    }

    cancel.store(true);
    const juce::TemporaryFile cancelled(".wav");
    return cancelled.getFile().replaceWithText("previous export")
        && blackie::renderVocalFile(input.getFile(), cancelled.getFile(), dry, cancel, progress).failed()
        && cancelled.getFile().loadFileAsString() == "previous export"
        && blackie::renderVocalFile(input.getFile(), input.getFile(), dry, cancel, progress).failed();
}
}

int main()
{
    if (!checkExport(48000) || !checkExport(44100))
    {
        std::cerr << "Desktop export failed its WAV alignment, resampling or cancellation check.\n";
        return 1;
    }
}
