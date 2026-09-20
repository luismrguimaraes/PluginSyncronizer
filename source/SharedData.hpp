#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <optional>

struct SharedData
{
    struct Listener
    {
        virtual void callbackLoadInner(const juce::PluginDescription& pluginDescription) = 0;
        virtual void callbackLoadInnerState(const juce::PluginDescription& pluginDescription, const juce::MemoryBlock& stateMemoryBlock) = 0;
        // ToDo: also add parameterBegin and end touch
        virtual void callbackUpdateParameterValue(int parameterIndex, float newValue) = 0;
        virtual void callbackRemoveInner() = 0;
    };

    std::optional<juce::PluginDescription> plugin_description;

    std::vector<SharedData::Listener*> listeners;
};

class SharedDataManager
{
public:
    SharedDataManager()
    {
        shared_data_->listeners.reserve(10);
    }

    void getStateInformation(juce::MemoryBlock& destData, juce::File logs_dir);
    void setStateInformation(const void* data, int sizeInBytes);

    void addListener(SharedData::Listener* newListener);
    void removeListener(SharedData::Listener* listenerToRemove);

    void updateInnerPd(const SharedData::Listener* sender, juce::PluginDescription pd);
    void updateInnerState(const SharedData::Listener* sender, juce::MemoryBlock memoryBlock);
    void updateParameterValue(const SharedData::Listener* sender, int parameterIndex, float newValue);

    void removeInner();

    std::optional<juce::PluginDescription> getPluginDescription();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedDataManager)

    juce::SharedResourcePointer<SharedData> shared_data_ {};
};