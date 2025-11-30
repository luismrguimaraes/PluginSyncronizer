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
    };

    std::unique_ptr<juce::AudioPluginInstance> inner_;
    juce::PluginDescription plugin_description_;

    std::vector<SharedData::Listener*> listeners_;
};

class SharedDataManager
{
public:
    SharedDataManager()
    {
        shared_data_->listeners_.reserve (10);
    }

    void addListener (SharedData::Listener* new_listener)
    {
        if (std::ranges::find (shared_data_->listeners_, new_listener) == shared_data_->listeners_.end())
        {
            shared_data_->listeners_.push_back (new_listener);
        }
    }

    void removeListener (SharedData::Listener* listener_to_remove)
    {
        auto it = std::ranges::find (shared_data_->listeners_, listener_to_remove);
        if (it != shared_data_->listeners_.end())
        {
            shared_data_->listeners_.erase (it);
        }
    }

    void updateInner (std::unique_ptr<juce::AudioPluginInstance> inner, juce::PluginDescription pd)
    {
        if (pd.isDuplicateOf (shared_data_->plugin_description_))
            return;

        shared_data_->inner_ = std::move (inner);
        shared_data_->plugin_description_ = pd;

        juce::MemoryBlock state_memory_block;
        shared_data_->inner_->getStateInformation (state_memory_block);

        for (auto& listener : shared_data_->listeners_)
        {
            listener->loadInnerState (shared_data_->plugin_description_, state_memory_block);
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SharedDataManager)

    juce::SharedResourcePointer<SharedData> shared_data_ {};
};