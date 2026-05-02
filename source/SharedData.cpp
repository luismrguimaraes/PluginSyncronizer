#include "SharedData.hpp"

void SharedDataManager::addListener(SharedData::Listener* newListener)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    const juce::ScopedLock sl(shared_data_mutex_);

    if (std::ranges::find(shared_data_->listeners, newListener) == shared_data_->listeners.end())
    {
        shared_data_->listeners.push_back(newListener);
    }
}

void SharedDataManager::removeListener(SharedData::Listener* listenerToRemove)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    const juce::ScopedLock sl(shared_data_mutex_);

    auto it = std::ranges::find(shared_data_->listeners, listenerToRemove);
    if (it != shared_data_->listeners.end())
    {
        shared_data_->listeners.erase(it);
    }
}

void SharedDataManager::updateInner([[maybe_unused]] const SharedData::Listener* sender, std::unique_ptr<juce::AudioPluginInstance> inner, juce::PluginDescription pd)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    const juce::ScopedLock sl(shared_data_mutex_);

    if (shared_data_->plugin_description.has_value() && pd.isDuplicateOf(shared_data_->plugin_description.value()))
    {
        jassert(shared_data_->inner != nullptr);
        return;
    }

    shared_data_->inner = std::move(inner);
    shared_data_->plugin_description = pd;

    juce::MemoryBlock state_memory_block;
    shared_data_->inner->getStateInformation(state_memory_block);

    for (auto& listener : shared_data_->listeners)
    {
        listener->callbackLoadInner(shared_data_->plugin_description.value(), state_memory_block);
    }
}

void SharedDataManager::updateInnerState(const SharedData::Listener* sender, juce::MemoryBlock stateMemoryBlock)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    const juce::ScopedLock sl(shared_data_mutex_);

    jassert(shared_data_->plugin_description.has_value());
    for (auto& listener : shared_data_->listeners)
    {
        if (listener == sender)
            continue;
        listener->callbackLoadInnerState(shared_data_->plugin_description.value(), stateMemoryBlock);
    }
}

void SharedDataManager::updateParameterValue(const SharedData::Listener* sender, int parameterIndex, float newValue)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    const juce::ScopedLock sl(shared_data_mutex_);

    for (auto& listener : shared_data_->listeners)
    {
        if (listener == sender)
            continue;
        listener->callbackUpdateParameterValue(parameterIndex, newValue);
    }
}

void SharedDataManager::removeInner()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    const juce::ScopedLock sl(shared_data_mutex_);

    if (shared_data_->inner == nullptr)
        return;

    shared_data_->inner = nullptr;
    shared_data_->plugin_description.reset();

    for (auto& listener : shared_data_->listeners)
    {
        listener->callbackRemoveInner();
    }
}