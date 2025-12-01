#pragma once

#include <__algorithm/ranges_find.h>
#include <__algorithm/ranges_includes.h>
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

struct SharedData
{
    struct Listener
    {
        virtual void loadInnerState (const juce::PluginDescription& plugin_description, const juce::MemoryBlock& memory_block) = 0;
        virtual void clearPlugin() = 0;
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

    void addListener (SharedData::Listener* new_listener);

    void removeListener (SharedData::Listener* listener_to_remove);

    void updateInner (std::unique_ptr<juce::AudioPluginInstance> inner, juce::PluginDescription pd);

    void removeInner();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SharedDataManager)

    juce::CriticalSection inner_mutex_;
    juce::SharedResourcePointer<SharedData> shared_data_ {};
};