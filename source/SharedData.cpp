#include "SharedData.hpp"
#include "utils.hpp"
#include <chrono>
#include <thread>

void SharedDataManager::getStateInformation(juce::MemoryBlock& destData, juce::File logs_directory)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::XmlElement xml { utils::kSharedStateTag };

    if (shared_data_->plugin_description.has_value())
    {
        xml.addChildElement(shared_data_->plugin_description.value().createXml().release());
    }

    xml.writeTo(logs_directory.getChildFile("SharedPluginStateLog.xml"));

    const auto xlm_as_string = xml.toString();
    destData.replaceAll(xlm_as_string.toRawUTF8(), xlm_as_string.getNumBytesAsUTF8());
}

void SharedDataManager::setStateInformation(const void* data, int sizeInBytes)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto xml = juce::XmlDocument::parse(juce::String(juce::CharPointer_UTF8(static_cast<const char*>(data)), static_cast<size_t>(sizeInBytes)));
    require(xml != nullptr);

    if (auto* plugin_description_node = xml->getChildByName("PLUGIN"))
    {
        juce::PluginDescription pd;
        pd.loadFromXml(*plugin_description_node);
        shared_data_->plugin_description = pd;

        for (auto& listener : shared_data_->listeners)
        {
            listener->callbackLoadInner(shared_data_->plugin_description.value());
        }
    }
}

void SharedDataManager::addListener(SharedData::Listener* newListener)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (std::ranges::find(shared_data_->listeners, newListener) == shared_data_->listeners.end())
    {
        shared_data_->listeners.push_back(newListener);
    }
}

void SharedDataManager::removeListener(SharedData::Listener* listenerToRemove)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto it = std::ranges::find(shared_data_->listeners, listenerToRemove);
    if (it != shared_data_->listeners.end())
    {
        shared_data_->listeners.erase(it);
    }
}

void SharedDataManager::updateInnerPd(const SharedData::Listener* sender, juce::PluginDescription pd)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (shared_data_->plugin_description.has_value() && pd.isDuplicateOf(shared_data_->plugin_description.value()))
        return;

    shared_data_->plugin_description = pd;

    for (auto& listener : shared_data_->listeners)
    {
        if (sender == listener)
            continue;
        listener->callbackLoadInner(shared_data_->plugin_description.value());
    }
}

void SharedDataManager::updateInnerState(const SharedData::Listener* sender, juce::MemoryBlock stateMemoryBlock)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    jassert(shared_data_->plugin_description.has_value());
    for (auto& listener : shared_data_->listeners)
    {
        if (sender == listener)
            continue;
        listener->callbackLoadInnerState(shared_data_->plugin_description.value(), stateMemoryBlock);
    }
}

void SharedDataManager::updateParameterValue(const SharedData::Listener* sender, int parameterIndex, float newValue)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    for (auto& listener : shared_data_->listeners)
    {
        if (sender == listener)
            continue;
        listener->callbackUpdateParameterValue(parameterIndex, newValue);
    }
}

void SharedDataManager::removeInner()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (!shared_data_->plugin_description.has_value())
        return;

    shared_data_->plugin_description.reset();

    for (auto& listener : shared_data_->listeners)
    {
        listener->callbackRemoveInner();
    }
}

std::optional<juce::PluginDescription> SharedDataManager::getPluginDescription()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    return shared_data_->plugin_description;
}
