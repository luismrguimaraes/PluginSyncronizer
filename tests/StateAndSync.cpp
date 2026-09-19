#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <chrono>
#include <filesystem>
#include <thread>

namespace
{
    constexpr int kAsyncInstanceLoadingWaitMs { 1000 };
    constexpr int kParamProcessingWaitMs { 100 };

    inline const juce::String kVST3FolderPath { "./plugins/VST3/" };
    inline const juce::String kCentaurVST3 { "./plugin-descriptions/chow_centaur_vst3.xml" };
    inline const juce::String kSalaVST3 { "./plugin-descriptions/sala_vst3.xml" };

    juce::PluginDescription get_pd_from_xml(juce::String xmlRelativePath)
    {
        REQUIRE(!juce::File::isAbsolutePath(xmlRelativePath));
        CAPTURE(xmlRelativePath);

        juce::File this_file { __FILE__ };
        auto this_dir = this_file.getParentDirectory();
        CAPTURE(this_dir.getFullPathName());

        auto xml_file = this_dir.getChildFile(xmlRelativePath);
        CAPTURE(xml_file.getFullPathName());
        REQUIRE(xml_file.getSize() != 0);
        auto xml = juce::parseXML(xml_file);
        REQUIRE(xml);

        auto file_name = xml->getStringAttribute("file");
        if (file_name.endsWith("vst3"))
        {
            auto plugin_path = this_dir.getChildFile(kVST3FolderPath).getChildFile(file_name);
            CAPTURE(plugin_path.getFullPathName());
            xml->setAttribute("file", plugin_path.getFullPathName());
        }
        else
        {
            UNSCOPED_INFO("Loading Plugin Description but this may fail remotely");
        }

        juce::PluginDescription pd;
        pd.loadFromXml(*xml);

        return pd;
    }

    void check_inner_loaded_and_pd_has_value(const PluginProcessor& processor)
    {
        CHECK(processor.getPluginDescription().has_value());
        CHECK(processor.isAnInnerLoaded());
    }
}

TEST_CASE("Loads state correctly", "[State]")
{
#ifndef TARGET_OS_OSX
    return;
#endif
    auto* messageManager = juce::MessageManager::getInstance();
    REQUIRE(messageManager != nullptr);

    auto pd = get_pd_from_xml(kSalaVST3);

    PluginProcessor processor1;

    runWithinPluginEditor(processor1, [pd](PluginProcessor& processor) {
        CHECK(!processor.getPluginDescription().has_value());
        processor.createSharedPluginInstance(pd);
        CHECK(processor.getPluginDescription().has_value());
    });

    CHECK(messageManager->runDispatchLoopUntil(kAsyncInstanceLoadingWaitMs));
    CHECK(processor1.getInnerParameter(1) != 0.45f);
    processor1.setInnerParameter(1, 0.45f);
    CHECK(messageManager->runDispatchLoopUntil(kAsyncInstanceLoadingWaitMs));
    CHECK(processor1.getInnerParameter(1) == 0.45f);

    // juce::MemoryBlock state;
    // processor1.getStateInformation(juce::MemoryBlock & destData)
}

TEST_CASE("Creating a new instance loads an inner if there is a shared plugin description", "[Sync]")
{
#ifndef TARGET_OS_OSX
    return;
#endif

    auto* messageManager = juce::MessageManager::getInstance();
    REQUIRE(messageManager != nullptr);

    auto pd = get_pd_from_xml(kCentaurVST3);

    PluginProcessor processor1;

    runWithinPluginEditor(processor1, [pd](PluginProcessor& processor) {
        CHECK(!processor.getPluginDescription().has_value());
        processor.createSharedPluginInstance(pd);
        CHECK(processor.getPluginDescription().has_value());
    });

    CHECK(messageManager->runDispatchLoopUntil(kAsyncInstanceLoadingWaitMs));

    PluginProcessor processor2;
    CHECK(processor2.getPluginDescription().has_value());
    CHECK(processor2.getPluginDescription()->isDuplicateOf(pd));

    CHECK(messageManager->runDispatchLoopUntil(kAsyncInstanceLoadingWaitMs));

    check_inner_loaded_and_pd_has_value(processor2);
}

TEST_CASE("Changing one instance inner params also change the other instances inner params (if the respective options are enabled)", "[Sync]")
{
#ifndef TARGET_OS_OSX
    return;
#endif

    auto* messageManager = juce::MessageManager::getInstance();
    REQUIRE(messageManager != nullptr);

    auto pd = get_pd_from_xml(kCentaurVST3);

    PluginProcessor processor1;
    PluginProcessor processor2;
    PluginProcessor processor3;

    runWithinPluginEditor(processor1, [pd](PluginProcessor& processor) {
        CHECK(!processor.getPluginDescription().has_value());
        processor.createSharedPluginInstance(pd);
        CHECK(processor.getPluginDescription().has_value());
    });

    CHECK(messageManager->runDispatchLoopUntil(kAsyncInstanceLoadingWaitMs));

    check_inner_loaded_and_pd_has_value(processor1);
    check_inner_loaded_and_pd_has_value(processor2);
    check_inner_loaded_and_pd_has_value(processor3);

    constexpr float kOriginalVal { 0.0 };
    constexpr float kIntermediateVal { 0.25 };
    constexpr float kLastVal { 1.0 };
    constexpr int kParamIndex { 5 };

    CHECK(processor1.getInnerParameter(kParamIndex) == kOriginalVal);
    CHECK(processor2.getInnerParameter(kParamIndex) == kOriginalVal);
    CHECK(processor3.getInnerParameter(kParamIndex) == kOriginalVal);

    processor1.setInnerParameter(kParamIndex, kIntermediateVal);
    processor2.setInnerParameter(kParamIndex, kIntermediateVal);
    processor3.setInnerParameter(kParamIndex, kIntermediateVal);
    CHECK(messageManager->runDispatchLoopUntil(kParamProcessingWaitMs));

    CHECK(processor1.getInnerParameter(kParamIndex) == kIntermediateVal);
    CHECK(processor2.getInnerParameter(kParamIndex) == kIntermediateVal);
    CHECK(processor3.getInnerParameter(kParamIndex) == kIntermediateVal);

    runWithinPluginEditor(processor1, [pd](PluginProcessor& processor) {
        processor.setPropagateChanges(true);
    });
    runWithinPluginEditor(processor2, [pd](PluginProcessor& processor) {
        processor.setAcceptChanges(true);
    });
    runWithinPluginEditor(processor3, [pd](PluginProcessor& processor) {
        processor.setAcceptChanges(false);
    });

    processor1.setInnerParameter(kParamIndex, kLastVal);
    CHECK(messageManager->runDispatchLoopUntil(kParamProcessingWaitMs));

    CHECK(processor1.getInnerParameter(kParamIndex) == kLastVal);
    CHECK(processor2.getInnerParameter(kParamIndex) == kLastVal);
    CHECK(processor3.getInnerParameter(kParamIndex) == kIntermediateVal);
}
