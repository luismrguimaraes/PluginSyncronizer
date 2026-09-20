#include "helpers/test_ui_helpers.h"
#include "helpers/test_utils.hpp"
#include "utils.hpp"

#include <PluginProcessor.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <chrono>
#include <filesystem>
#include <thread>

TEST_CASE("Loads state correctly", "[State]")
{
#ifndef JUCE_MAC
    return;
#endif
    auto* messageManager = juce::MessageManager::getInstance();
    REQUIRE(messageManager != nullptr);

    auto pd = get_pd_from_xml(kSalaVST3);
    juce::MemoryBlock state_1st;

    {
        PluginProcessor processor1;

        runWithinPluginEditor(processor1, [pd](PluginProcessor& processor) {
            CHECK(!processor.getPluginDescription().has_value());
            processor.createSharedPluginInstance(pd);
            CHECK(processor.getPluginDescription().has_value());
        });

        CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));
        CHECK(!juce::exactlyEqual(processor1.getInnerParameter(1), 0.45f));
        processor1.setInnerParameter(1, 0.45f);
        CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));
        CHECK(juce::exactlyEqual(processor1.getInnerParameter(1), 0.45f));

        processor1.getStateInformation(state_1st);
    }
    REQUIRE(!state_1st.isEmpty());

    auto xml_1st = parse_state(state_1st);
    PluginProcessor processor1;
    processor1.setStateInformation(state_1st.getData(), static_cast<int>(state_1st.getSize()));
    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

    juce::MemoryBlock state_2nd;
    processor1.getStateInformation(state_2nd);
    auto xml_2nd = parse_state(state_2nd);

    xml_1st->getChildByName("PLUGIN")->setAttribute("infoUpdateTime", 0);
    xml_2nd->getChildByName("PLUGIN")->setAttribute("infoUpdateTime", 0);
    CHECK(xml_1st->toString() == xml_2nd->toString());
}

TEST_CASE("Loads state correctly when there is more than one instance", "[State]")
{
#ifndef JUCE_MAC
    return;
#endif
    auto* messageManager = juce::MessageManager::getInstance();
    REQUIRE(messageManager != nullptr);

    auto pd = get_pd_from_xml(kSalaVST3);
    juce::MemoryBlock state1_1st;
    juce::MemoryBlock state2_1st;
    juce::MemoryBlock state3_1st;

    {
        PluginProcessor processor1;
        PluginProcessor processor2;
        PluginProcessor processor3;

        runWithinPluginEditor(processor1, [pd](PluginProcessor& processor) {
            CHECK(!processor.getPluginDescription().has_value());
            processor.createSharedPluginInstance(pd);
            CHECK(processor.getPluginDescription().has_value());
        });
        runWithinPluginEditor(processor2, [pd](PluginProcessor& processor) {
            processor.setAcceptChanges(true);
        });
        runWithinPluginEditor(processor3, [pd](PluginProcessor& processor) {
            processor.setAcceptChanges(true);
        });

        CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));
        CHECK(!juce::exactlyEqual(processor1.getInnerParameter(1), 0.45f));
        processor1.setInnerParameter(1, 0.45f);
        CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));
        CHECK(juce::exactlyEqual(processor1.getInnerParameter(1), 0.45f));

        processor1.getStateInformation(state1_1st);
        processor2.getStateInformation(state2_1st);
        processor3.getStateInformation(state3_1st);
    }
    REQUIRE(!state1_1st.isEmpty());
    REQUIRE(!state2_1st.isEmpty());
    REQUIRE(!state3_1st.isEmpty());

    auto check = [messageManager](PluginProcessor& processor, const juce::MemoryBlock& filled_state, juce::MemoryBlock& state_to_fill) {
        REQUIRE(!filled_state.isEmpty());
        REQUIRE(state_to_fill.isEmpty());

        apply_state(processor, filled_state);
        CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

        processor.getStateInformation(state_to_fill);

        auto xml_1st = parse_state(filled_state);
        auto xml_2nd = parse_state(state_to_fill);
        xml_1st->getChildByName("PLUGIN")->setAttribute("infoUpdateTime", 0);
        xml_2nd->getChildByName("PLUGIN")->setAttribute("infoUpdateTime", 0);
        CHECK(xml_1st->toString() == xml_2nd->toString());
    };

    PluginProcessor processor1;
    juce::MemoryBlock state1_2nd;
    check(processor1, state1_1st, state1_2nd);
    PluginProcessor processor2;
    juce::MemoryBlock state2_2nd;
    check(processor2, state2_1st, state2_2nd);
    PluginProcessor processor3;
    juce::MemoryBlock state3_2nd;
    check(processor3, state3_1st, state3_2nd);

    auto xml1 = parse_state(state1_2nd);
    auto xml2 = parse_state(state2_2nd);
    auto xml3 = parse_state(state3_2nd);
    xml1->getChildByName("PLUGIN")->setAttribute("infoUpdateTime", 0);
    xml2->getChildByName("PLUGIN")->setAttribute("infoUpdateTime", 0);
    xml3->getChildByName("PLUGIN")->setAttribute("infoUpdateTime", 0);
    CHECK(xml1->toString() == xml2->toString());
    CHECK(xml2->toString() == xml3->toString());
}

TEST_CASE("Creating a new instance loads an inner if there is a shared plugin description", "[Sync]")
{
#ifndef JUCE_MAC
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

    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

    PluginProcessor processor2;
    CHECK(processor2.getPluginDescription().has_value());
    CHECK(processor2.getPluginDescription()->isDuplicateOf(pd));

    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

    check_inner_loaded_and_pd_has_value(processor2);
}

TEST_CASE("Changing one instance inner params also change the other instances inner params (if the respective options are enabled)", "[Sync]")
{
#ifndef JUCE_MAC
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

    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

    check_inner_loaded_and_pd_has_value(processor1);
    check_inner_loaded_and_pd_has_value(processor2);
    check_inner_loaded_and_pd_has_value(processor3);

    constexpr float kOriginalVal { 0.0 };
    constexpr float kIntermediateVal { 0.25 };
    constexpr float kLastVal { 1.0 };
    constexpr int kParamIndex { 5 };

    CHECK(juce::exactlyEqual(processor1.getInnerParameter(kParamIndex), kOriginalVal));
    CHECK(juce::exactlyEqual(processor2.getInnerParameter(kParamIndex), kOriginalVal));
    CHECK(juce::exactlyEqual(processor3.getInnerParameter(kParamIndex), kOriginalVal));

    processor1.setInnerParameter(kParamIndex, kIntermediateVal);
    processor2.setInnerParameter(kParamIndex, kIntermediateVal);
    processor3.setInnerParameter(kParamIndex, kIntermediateVal);
    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

    CHECK(juce::exactlyEqual(processor1.getInnerParameter(kParamIndex), kIntermediateVal));
    CHECK(juce::exactlyEqual(processor2.getInnerParameter(kParamIndex), kIntermediateVal));
    CHECK(juce::exactlyEqual(processor3.getInnerParameter(kParamIndex), kIntermediateVal));

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
    CHECK(messageManager->runDispatchLoopUntil(kAsyncCallWaitMs));

    CHECK(juce::exactlyEqual(processor1.getInnerParameter(kParamIndex), kLastVal));
    CHECK(juce::exactlyEqual(processor2.getInnerParameter(kParamIndex), kLastVal));
    CHECK(juce::exactlyEqual(processor3.getInnerParameter(kParamIndex), kIntermediateVal));
}