#include "SharedData.hpp"

void SharedDataManager::addListener (SharedData::Listener* newListener)
{
    if (std::ranges::find (shared_data_->listeners, newListener) == shared_data_->listeners.end())
    {
        shared_data_->listeners.push_back (newListener);
    }
}

void SharedDataManager::removeListener (SharedData::Listener* listenerToRemove)
{
    auto it = std::ranges::find (shared_data_->listeners, listenerToRemove);
    if (it != shared_data_->listeners.end())
    {
        shared_data_->listeners.erase (it);
    }
}

void SharedDataManager::updateInner (std::unique_ptr<juce::AudioPluginInstance> inner, juce::PluginDescription pd)
{
    const juce::ScopedLock sl (inner_mutex_);

    if (shared_data_->plugin_description.has_value() && pd.isDuplicateOf (shared_data_->plugin_description.value()))
    {
        jassert (shared_data_->inner != nullptr);
        return;
    }

    shared_data_->inner = std::move (inner);
    shared_data_->plugin_description = pd;

    juce::MemoryBlock state_memory_block;
    shared_data_->inner->getStateInformation (state_memory_block);

    for (auto& listener : shared_data_->listeners)
    {
        listener->loadInner (shared_data_->plugin_description.value(), state_memory_block);
    }
}

void SharedDataManager::updateInnerState (juce::MemoryBlock stateMemoryBlock)
{
    jassert (shared_data_->plugin_description.has_value());
    for (auto& listener : shared_data_->listeners)
    {
        listener->loadInnerState (shared_data_->plugin_description.value(), stateMemoryBlock);
    }
}

void SharedDataManager::removeInner()
{
    const juce::ScopedLock sl (inner_mutex_);

    if (shared_data_->inner == nullptr)
        return;

    shared_data_->inner = nullptr;
    shared_data_->plugin_description.reset();

    for (auto& listener : shared_data_->listeners)
    {
        listener->removeInner();
    }
}