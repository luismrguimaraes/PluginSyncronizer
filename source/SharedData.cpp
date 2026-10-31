#include "SharedData.hpp"
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>

namespace
{
    inline const juce::String kPluginDescriptionsTag { "plugin_descriptions" };
    inline const juce::String kPDElemTagPrefix { "PLUGIN_" };
}

void SharedDataManager::getStateInformation(juce::MemoryBlock& destData, const juce::File& logs_directory) const
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::XmlElement shared_data_state { utils::kSharedStateTag };

    auto plugin_descriptions = std::make_unique<juce::XmlElement>(kPluginDescriptionsTag);
    for (size_t group = 0; group < utils::kGroupsCount; ++group)
    {
        if (shared_data_->plugin_descriptions[group].has_value())
        {
            auto pd_xml = shared_data_->plugin_descriptions[group].value().createXml();
            pd_xml->setTagName(kPDElemTagPrefix + juce::String { group });
            plugin_descriptions->addChildElement(pd_xml.release());
        }
        else
        {
            auto empty_xml = std::make_unique<juce::XmlElement>(kPDElemTagPrefix + juce::String { group });
            plugin_descriptions->addChildElement(empty_xml.release());
        }
    }
    shared_data_state.addChildElement(plugin_descriptions.release());

    shared_data_state.writeTo(logs_directory.getChildFile("SharedPluginState.xml"));

    const auto xlm_as_string = shared_data_state.toString();
    destData.replaceAll(xlm_as_string.toRawUTF8(), xlm_as_string.getNumBytesAsUTF8());
}

void SharedDataManager::setStateInformation(const void* data, int sizeInBytes, const juce::File& logs_directory)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto shared_data_state = juce::XmlDocument::parse(juce::String(juce::CharPointer_UTF8(static_cast<const char*>(data)), static_cast<size_t>(sizeInBytes)));
    require(shared_data_state != nullptr);
    shared_data_state->writeTo(logs_directory.getChildFile("LoadedSharedPluginState.xml"));

    if (auto* plugin_descriptions = shared_data_state->getChildByName(kPluginDescriptionsTag))
    {
        for (size_t group = 0; group < utils::kGroupsCount; ++group)
        {
            auto pd_xml = plugin_descriptions->getChildByName(kPDElemTagPrefix + juce::String { group });
            require(pd_xml != nullptr);
            if (pd_xml->getNumAttributes() > 0)
            {
                pd_xml->setTagName("PLUGIN"); // tag that loadFromXml expects
                juce::PluginDescription pd;
                pd.loadFromXml(*pd_xml);
                shared_data_->plugin_descriptions[group] = pd;
            }
            else
            {
                shared_data_->plugin_descriptions[group].reset();
            }

            for (auto& listener : shared_data_->listeners)
            {
                if (group != listener->getGroup())
                    continue;
                if (!shared_data_->plugin_descriptions[group].has_value())
                {
                    listener->callbackRemoveInner();
                    continue;
                }
                listener->callbackLoadInner(shared_data_->plugin_descriptions[group].value());
            }
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

    const auto sender_group = sender->getGroup();
    require(0 <= sender_group && sender_group < utils::kGroupsCount);

    if (shared_data_->plugin_descriptions[sender_group].has_value() && utils::areDuplicates(pd, shared_data_->plugin_descriptions[sender_group].value()))
        return;

    shared_data_->plugin_descriptions[sender_group] = pd;

    for (auto& listener : shared_data_->listeners)
    {
        if (sender == listener || sender_group != listener->getGroup())
            continue;
        listener->callbackLoadInner(shared_data_->plugin_descriptions[sender_group].value());
    }
}

void SharedDataManager::updateInnerState(const SharedData::Listener* sender, juce::MemoryBlock stateMemoryBlock)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto sender_group = sender->getGroup();
    require(0 <= sender_group && sender_group < utils::kGroupsCount);

    jassert(shared_data_->plugin_descriptions[sender_group].has_value());
    for (auto& listener : shared_data_->listeners)
    {
        if (sender == listener || sender_group != listener->getGroup())
            continue;
        listener->callbackLoadInnerState(shared_data_->plugin_descriptions[sender_group].value(), stateMemoryBlock);
    }
}

void SharedDataManager::updateParameterValue(const SharedData::Listener* sender, int parameterIndex, float newValue)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto sender_group = sender->getGroup();
    require(0 <= sender_group && sender_group < utils::kGroupsCount);

    for (auto& listener : shared_data_->listeners)
    {
        if (sender == listener || sender_group != listener->getGroup())
            continue;
        listener->callbackUpdateParameterValue(parameterIndex, newValue);
    }
}

void SharedDataManager::removeInner(const SharedData::Listener* sender)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto sender_group = sender->getGroup();
    require(0 <= sender_group && sender_group < utils::kGroupsCount);

    if (!shared_data_->plugin_descriptions[sender_group].has_value()) return;

    shared_data_->plugin_descriptions[sender_group].reset();

    for (auto& listener : shared_data_->listeners)
    {
        if (sender_group != listener->getGroup())
            continue;
        listener->callbackRemoveInner();
    }
}

std::optional<juce::PluginDescription> SharedDataManager::getPluginDescription(size_t group)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    require(0 <= group && group < utils::kGroupsCount);
    return shared_data_->plugin_descriptions[group];
}
