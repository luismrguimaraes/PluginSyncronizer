#include "helpers/test_ui_helpers.h"
#include "helpers/test_utils.hpp"

#include <PluginProcessor.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE("Prepare changes inner sample rate and block size", "[Basics]")
{
#ifndef JUCE_MAC
    return;
#endif

    auto* messageManager = juce::MessageManager::getInstance();
    REQUIRE(messageManager != nullptr);

    auto pd = get_pd_from_xml(kCentaurVST3);

    PluginProcessor processor1;
    runWithinPluginEditor(processor1, [pd](PluginProcessor& processor) {
        processor.createSharedPluginInstance(pd);
    });
    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

    CHECK(!juce::exactlyEqual(processor1.getInner()->getSampleRate(), 12000.0));
    CHECK(!juce::exactlyEqual(processor1.getInner()->getBlockSize(), 512));

    processor1.prepareToPlay(12000, 512);
    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

    CHECK(juce::exactlyEqual(processor1.getInner()->getSampleRate(), 12000.0));
    CHECK(juce::exactlyEqual(processor1.getInner()->getBlockSize(), 512));
}

TEST_CASE("Sine passes clean without an inner", "[Basics]")
{
    auto* messageManager = juce::MessageManager::getInstance();
    REQUIRE(messageManager != nullptr);

    constexpr float kSampleRate { 44100 };
    juce::AudioBuffer<float> sine_buffer { 1, 512 };

    auto data = sine_buffer.getWritePointer(0);
    for (int i = 0; i < sine_buffer.getNumSamples(); ++i)
    {
        float phase = i / kSampleRate * juce::MathConstants<float>::twoPi;
        data[i] = std::sin(440 * phase);
    }

    PluginProcessor processor1;
    processor1.prepareToPlay(kSampleRate, 512);
    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

    juce::MidiBuffer midi_buffer;
    processor1.processBlock(sine_buffer, midi_buffer);

    bool all_samples_expected { true };
    for (int i = 0; i < sine_buffer.getNumSamples(); ++i)
    {
        float phase = i / kSampleRate * juce::MathConstants<float>::twoPi;
        auto expected = std::sin(440 * phase);
        if (!juce::exactlyEqual(data[i], expected))
        {
            all_samples_expected = false;
        }
    }
    CHECK(all_samples_expected);
}