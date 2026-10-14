#include <PluginProcessor.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE("one is equal to one", "")
{
    REQUIRE(1 == 1);
}

TEST_CASE("Plugin instance", "")
{
    PluginProcessor testPlugin;

    SECTION("name")
    {
        CHECK_THAT(testPlugin.getName().toStdString(),
            Catch::Matchers::Equals("Plugin Syncronizer"));
    }
}

#ifdef PAMPLEJUCE_IPP
    #include <ipp.h>

TEST_CASE("IPP version", "")
{
    #if defined(__APPLE__)
    // macOS uses 2021.9.1 from pip wheel (only x86_64 version available)
    CHECK_THAT(ippsGetLibVersion()->Version, Catch::Matchers::Equals("2021.9.1 (r0x7e208212)"));
    #elif WIN32
    CHECK_THAT(ippsGetLibVersion()->Version, Catch::Matchers::Equals("2022.3.0 (r0x0fc08bb1)"));
    #endif
    // @LG: ignore Linux version since I was unable to force a version for Linux on CI
}
#endif