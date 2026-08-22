#include "DebugUtils.hpp"
#include "juce_core/system/juce_PlatformDefs.h"

namespace
{
    void readJuceBuffer(const juce::AudioBuffer<float>& buffer, std::vector<std::vector<float>>& debugger)
    {
        auto num_channels = static_cast<size_t>(buffer.getNumChannels());
        auto num_samples = static_cast<size_t>(buffer.getNumSamples());

        // if this is hit we might allocate in process
        jassert(num_channels <= debugger.size());

        debugger.resize(num_channels);
        for (size_t ch = 0; ch < num_channels; ++ch)
        {
            // if this is hit we might allocate in process
            jassert(num_channels <= debugger[ch].size());

            debugger[ch].resize(num_samples);
            for (size_t i = 0; i < num_samples; ++i)
            {
                debugger[ch][i] = buffer.getSample(static_cast<int>(ch), static_cast<int>(i));
            }
        }
    }
}

void JuceDebugger::prepareToPlay(int samplesPerBlock, int inputChannels, int outputChannels)
{
    auto channel_count_to_resize_to = static_cast<size_t>(std::max(inputChannels, outputChannels));

    juce_input_buffer_debugger_.resize(channel_count_to_resize_to);
    for (auto& buffer : juce_input_buffer_debugger_)
    {
        buffer.resize(static_cast<size_t>(samplesPerBlock));
    }
    juce_output_buffer_debugger_.resize(channel_count_to_resize_to);
    for (auto& buffer : juce_output_buffer_debugger_)
    {
        buffer.resize(static_cast<size_t>(samplesPerBlock));
    }
}

void JuceDebugger::processInput(const juce::AudioBuffer<float>& buffer)
{
    readJuceBuffer(buffer, juce_input_buffer_debugger_);
}

void JuceDebugger::processOutput(const juce::AudioBuffer<float>& buffer)
{
    readJuceBuffer(buffer, juce_output_buffer_debugger_);
}
