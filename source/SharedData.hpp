#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

struct SharedData
{
    struct Listener
    {
        virtual void callbackLoadInner (const juce::PluginDescription& plugin_description, const juce::MemoryBlock& memory_block) = 0;
        virtual void callbackLoadInnerState (const juce::PluginDescription& plugin_description, const juce::MemoryBlock& state_memory_block) = 0;
        // ToDo: also add parameterBegin and end touch
        virtual void callbackUpdateParameterValue (int parameterIndex, float newValue) = 0;
        virtual void callbackRemoveInner() = 0;
    };

    std::unique_ptr<juce::AudioPluginInstance> inner;
    std::optional<juce::PluginDescription> plugin_description;

    std::vector<SharedData::Listener*> listeners;
};

class SharedDataManager
{
public:
    SharedDataManager()
    {
        shared_data_->listeners.reserve (10);
    }

    void addListener (SharedData::Listener* newListener);

    void removeListener (SharedData::Listener* listenerToRemove);

    void updateInner (const SharedData::Listener* sender, std::unique_ptr<juce::AudioPluginInstance> inner, juce::PluginDescription pd);
    void updateInnerState (const SharedData::Listener* sender, juce::MemoryBlock memoryBlock);
    void updateParameterValue (const SharedData::Listener* sender, int parameterIndex, float newValue);

    void removeInner();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SharedDataManager)

    juce::CriticalSection inner_mutex_;
    juce::SharedResourcePointer<SharedData> shared_data_ {};
};