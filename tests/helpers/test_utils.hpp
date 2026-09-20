#include <PluginProcessor.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

static constexpr int kAsyncCallWaitMs { 500 };

static inline const juce::String kCentaurVST3 { "./plugin-descriptions/chow_centaur_vst3.xml" };
static inline const juce::String kSalaVST3 { "./plugin-descriptions/sala_vst3.xml" };

juce::PluginDescription get_pd_from_xml(juce::String xmlRelativePath);

void check_inner_loaded_and_pd_has_value(const PluginProcessor& processor);

void apply_state(PluginProcessor& processor, const juce::MemoryBlock& state);

std::unique_ptr<juce::XmlElement> parse_state(const juce::MemoryBlock& state);