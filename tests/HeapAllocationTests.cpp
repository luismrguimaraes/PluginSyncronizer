#include "helpers/test_ui_helpers.h"
#include "helpers/test_utils.hpp"

#include <PluginProcessor.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdlib>
#include <new>

// chatGPT allocation tracker implementation

#include <cstddef>
#include <cstdlib>
#include <new>

namespace
{
    thread_local std::size_t allocationCount = 0;

    struct AllocationScopedTracker
    {
        AllocationScopedTracker()
            : startCount(allocationCount)
        {
        }

        [[nodiscard]] std::size_t count() const noexcept
        {
            return allocationCount - startCount;
        }

        void reset() noexcept
        {
            startCount = allocationCount;
        }

    private:
        std::size_t startCount;
    };
}

void* operator new(std::size_t size)
{
    if (void* ptr = std::malloc(size))
    {
        ++allocationCount;
        return ptr;
    }

    throw std::bad_alloc {};
}

void operator delete(void* ptr) noexcept
{
    std::free(ptr);
}

void* operator new[](std::size_t size)
{
    if (void* ptr = std::malloc(size))
    {
        ++allocationCount;
        return ptr;
    }

    throw std::bad_alloc {};
}

void operator delete[](void* ptr) noexcept
{
    std::free(ptr);
}

// Sized delete overloads
void operator delete(void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}

TEST_CASE("Allocation test", "[HeapAllocation]")
{
#ifndef JUCE_MAC
    return;
#endif

    auto* messageManager = juce::MessageManager::getInstance();
    REQUIRE(messageManager != nullptr);

    PluginProcessor processor1;
    PluginProcessor processor2;

    AllocationScopedTracker non_process_allocations;

    auto pd = get_pd_from_xml(kSalaVST3);
    runWithinPluginEditor(processor1, [pd](PluginProcessor& processor) {
        CHECK(!processor.getPluginDescription().has_value());
        processor.createSharedPluginInstance(pd);
        CHECK(processor.getPluginDescription().has_value());
    });
    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));
    check_inner_loaded_and_pd_has_value(processor1);
    check_inner_loaded_and_pd_has_value(processor2);

    processor1.prepareToPlay(44100, 512);
    processor2.prepareToPlay(44100, 512);
    juce::AudioBuffer<float> audio_buffer { 1, 512 };
    auto audio_data = audio_buffer.getWritePointer(0);
    for (auto i = 0; i < audio_buffer.getNumSamples(); i++)
    {
        audio_data[i] = static_cast<float>(i) / audio_buffer.getNumSamples();
    }

    JuceDebugger juce_debugger;
    juce_debugger.prepareToPlay(512, 1, 1);
    juce_debugger.readInput(audio_buffer);

    juce::MidiBuffer midi_buffer;

    CHECK(non_process_allocations.count() != 0);

    AllocationScopedTracker process_allocations;
    processor1.processBlock(audio_buffer, midi_buffer);
    CHECK(process_allocations.count() == 0);
    CHECK(!is_silence(audio_buffer));

    runWithinPluginEditor(processor1, [](PluginProcessor& processor) {
        auto inner = processor.getInner();
        AllocationScopedTracker allocation_tracker;
        processor.audioProcessorParameterChanged(inner, 1, 0.45f);
        CHECK(allocation_tracker.count() == 0);

        juce::AudioProcessor::ChangeDetails change_details;
        change_details = change_details.withLatencyChanged(true);

        allocation_tracker.reset();
        processor.audioProcessorChanged(inner, change_details);
        CHECK(allocation_tracker.count() == 0);
    });
}
