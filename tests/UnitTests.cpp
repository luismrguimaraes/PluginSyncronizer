#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "helpers/test_utils.hpp"

TEST_CASE("Plugin description duplicate checking", "[plugin_description]")
{
#ifdef JUCE_WINDOWS
    auto pd_1 = get_pd_from_xml("./plugin-descriptions/for_duplicate_checking/windows/spectral_panner_vst3_1.xml");
    auto pd_2 = get_pd_from_xml("./plugin-descriptions/for_duplicate_checking/windows/spectral_panner_vst3_2.xml");
    auto pd_3 = get_pd_from_xml("./plugin-descriptions/for_duplicate_checking/windows/spectral_panner_vst3_3.xml");
#else
    auto pd_1 = get_pd_from_xml("./plugin-descriptions/for_duplicate_checking/macos & linux/spectral_panner_vst3_1.xml");
    auto pd_2 = get_pd_from_xml("./plugin-descriptions/for_duplicate_checking/macos & linux/spectral_panner_vst3_2.xml");
    auto pd_3 = get_pd_from_xml("./plugin-descriptions/for_duplicate_checking/macos & linux/spectral_panner_vst3_3.xml");
#endif

    CHECK(utils::areDuplicates(pd_1, pd_2));
    CHECK(!utils::areDuplicates(pd_1, pd_3));
    CHECK(!utils::areDuplicates(pd_2, pd_3));
}