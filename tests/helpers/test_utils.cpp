#include "test_utils.hpp"

namespace
{
    inline const juce::String kVST3FolderPath { "./plugins/VST3/" };
}

juce::PluginDescription get_pd_from_xml(juce::String xmlRelativePath)
{
    REQUIRE(!juce::File::isAbsolutePath(xmlRelativePath));
    CAPTURE(xmlRelativePath);

    juce::File this_file { __FILE__ };
    auto tests_dir = this_file.getParentDirectory().getParentDirectory();
    CAPTURE(tests_dir.getFullPathName());

    auto xml_file = tests_dir.getChildFile(xmlRelativePath);
    CAPTURE(xml_file.getFullPathName());
    REQUIRE(xml_file.getSize() != 0);
    auto xml = juce::parseXML(xml_file);
    REQUIRE(xml);

    auto file_name = xml->getStringAttribute("file");
    if (file_name.endsWith("vst3"))
    {
        auto plugin_path = tests_dir.getChildFile(kVST3FolderPath).getChildFile(file_name);
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

void apply_state(PluginProcessor& processor, const juce::MemoryBlock& state)
{
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
}

std::unique_ptr<juce::XmlElement> parse_state(const juce::MemoryBlock& state)
{
    auto xml = juce::XmlDocument::parse(juce::String(juce::CharPointer_UTF8(static_cast<const char*>(state.getData())), static_cast<size_t>(state.getSize())));
    return xml;
}