#include "VocalCleanup.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
class VocalBoxProcessor final : public juce::AudioProcessor
{
public:
    VocalBoxProcessor()
        : AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::mono(), true)
            .withOutput("Output", juce::AudioChannelSet::mono(), true)),
          parameters(*this, nullptr, "VocalBox", createParameters())
    {
    }

    const juce::String getName() const override { return "Blackie Vocal Box"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void releaseResources() override {}

    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {
        const auto input = layout.getMainInputChannelSet();
        return input == layout.getMainOutputChannelSet()
            && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
    }

    void prepareToPlay(double sampleRate, int) override
    {
        for (auto& channel : channels)
            channel.prepare(sampleRate);

        setLatencySamples(channels.front().latency());
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        const blackie::Settings settings { noise->load(), room->load(), tone->load() };
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* samples = buffer.getWritePointer(channel);
            for (int index = 0; index < buffer.getNumSamples(); ++index)
                samples[index] = channels[static_cast<size_t>(channel)].process(samples[index], settings);
        }
    }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override
    {
        return new juce::GenericAudioProcessorEditor(*this);
    }

    void getStateInformation(juce::MemoryBlock& data) override
    {
        if (const auto xml = parameters.copyState().createXml())
            copyXmlToBinary(*xml, data);
    }

    void setStateInformation(const void* data, int size) override
    {
        if (const auto xml = getXmlFromBinary(data, size))
            if (xml->hasTagName(parameters.state.getType()))
                parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        const blackie::Settings defaults;
        const auto add = [&layout](const char* id, const char* name, float value)
        {
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> { 0, 1 }, value));
        };
        add("noise", "AI Noise (48 kHz)", defaults.noise);
        add("room", "Room Tail", defaults.room);
        add("tone", "Natural EQ", defaults.tone);
        return layout;
    }

    std::array<blackie::VocalCleanup, 2> channels;
    juce::AudioProcessorValueTreeState parameters;
    const std::atomic<float>* noise = parameters.getRawParameterValue("noise");
    const std::atomic<float>* room = parameters.getRawParameterValue("room");
    const std::atomic<float>* tone = parameters.getRawParameterValue("tone");
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalBoxProcessor();
}
