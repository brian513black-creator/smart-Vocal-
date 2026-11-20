#include "FileRender.h"

#include <algorithm>
#include <cmath>

namespace blackie
{
namespace
{
class FileSource final : public juce::AudioSource
{
public:
    explicit FileSource(juce::AudioFormatReader& input) : reader(input) {}

    void prepareToPlay(int, double) override { position = 0; }
    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& block) override
    {
        block.clearActiveBufferRegion();
        const int samples = static_cast<int>(std::min<juce::int64>(block.numSamples, reader.lengthInSamples - position));
        if (samples == 0 || failed)
            return;

        failed = !reader.read(block.buffer, block.startSample, samples, position, true, true);
        position += samples;
    }

    bool failed = false;

private:
    juce::AudioFormatReader& reader;
    juce::int64 position = 0;
};
}

juce::Result renderVocalFile(const juce::File& input, const juce::File& output,
                            RenderSettings settings, const std::atomic<bool>& cancel,
                            std::atomic<double>& progress)
{
    if (input == output)
        return juce::Result::fail("Choose an output file different from the source.");

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(input));
    if (!reader || reader->lengthInSamples <= 0)
        return juce::Result::fail("Could not read audio from the selected file.");

    if (reader->numChannels < 1 || reader->numChannels > 2
        || !(reader->sampleRate >= 8000 && reader->sampleRate <= 384000))
        return juce::Result::fail("Choose a mono or stereo file between 8 and 384 kHz.");

    constexpr double renderRate = 48000;
    constexpr int blockSize = 4096;
    const auto channels = static_cast<int>(reader->numChannels);
    const auto frames = std::llround(reader->lengthInSamples * renderRate / reader->sampleRate);
    if (frames == 0)
        return juce::Result::fail("The audio is too short to export.");

    FileSource source(*reader);
    juce::ResamplingAudioSource resampler(&source, false, channels);
    resampler.setResamplingRatio(reader->sampleRate / renderRate);
    juce::AudioSource& signal = reader->sampleRate == renderRate
                              ? static_cast<juce::AudioSource&>(source) : resampler;
    signal.prepareToPlay(blockSize, renderRate);

    juce::TemporaryFile temporary(output);
    auto stream = temporary.getFile().createOutputStream();
    if (!stream)
        return juce::Result::fail("Could not open the destination for writing.");

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.get(), renderRate, reader->numChannels, 32, {}, 0));
    if (!writer)
        return juce::Result::fail("Could not create the WAV file.");
    stream.release();

    std::array<VocalCleanup, 2> cleanup;
    for (int channel = 0; channel < channels; ++channel)
        cleanup[channel].prepare(renderRate, settings.cleanup);

    ReferenceEq autoEq;
    autoEq.prepare(renderRate);
    const auto latency = cleanup.front().latency();

    juce::AudioBuffer<float> buffer(channels, blockSize);
    juce::ScopedNoDenormals noDenormals;
    for (juce::int64 offset = 0; offset < frames + latency;)
    {
        if (cancel.load())
            return juce::Result::fail("Export cancelled.");

        const int count = static_cast<int>(std::min<juce::int64>(blockSize, frames + latency - offset));
        const int inputCount = static_cast<int>(std::clamp<juce::int64>(frames - offset, 0, count));
        buffer.clear();
        if (inputCount > 0)
            signal.getNextAudioBlock({ &buffer, 0, inputCount });
        if (source.failed)
            return juce::Result::fail("Could not decode the complete input file.");

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* samples = buffer.getWritePointer(channel);
            for (int sample = 0; sample < count; ++sample)
                samples[sample] = cleanup[channel].process(samples[sample], settings.cleanup);
        }

        const int skip = static_cast<int>(std::clamp<juce::int64>(latency - offset, 0, count));
        auto outputBlock = buffer.getArrayOfWritePointers();
        float* aligned[] { outputBlock[0] + skip, channels == 2 ? outputBlock[1] + skip : nullptr };
        autoEq.process(aligned, channels, count - skip, settings.autoEq);
        if (count > skip && !writer->writeFromAudioSampleBuffer(buffer, skip, count - skip))
            return juce::Result::fail("Could not finish writing the WAV file.");

        offset += count;
        progress.store(static_cast<double>(offset) / static_cast<double>(frames + latency));
    }

    if (!writer->flush())
        return juce::Result::fail("Could not finalize the WAV file.");
    writer.reset();
    if (cancel.load())
        return juce::Result::fail("Export cancelled.");
    if (!temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not save the export at the selected destination.");

    return juce::Result::ok();
}
}
