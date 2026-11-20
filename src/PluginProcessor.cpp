#include "ReferenceEq.h"
#include "VocalCleanup.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
constexpr auto matchBrianId = "matchBrian";

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

        referenceEq.prepare(sampleRate);
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

        referenceEq.process(buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                            buffer.getNumSamples(), matchBrian->load());
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
        if (const auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(parameters.state.getType()))
        {
            const auto state = juce::ValueTree::fromXml(*xml);
            const bool hasMatchAmount = state.getChildWithProperty("id", matchBrianId).isValid();
            parameters.replaceState(state);
            if (!hasMatchAmount)
                parameters.getParameter(matchBrianId)->setValueNotifyingHost(0.0f);
        }
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
        add(matchBrianId, "Auto EQ (Brian)", blackie::ReferenceEq::defaultAmount);
        return layout;
    }

    std::array<blackie::VocalCleanup, 2> channels;
    blackie::ReferenceEq referenceEq;
    juce::AudioProcessorValueTreeState parameters;
    const std::atomic<float>* noise = parameters.getRawParameterValue("noise");
    const std::atomic<float>* room = parameters.getRawParameterValue("room");
    const std::atomic<float>* tone = parameters.getRawParameterValue("tone");
    const std::atomic<float>* matchBrian = parameters.getRawParameterValue(matchBrianId);
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalBoxProcessor();
}
