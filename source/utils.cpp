#include "utils.hpp"

namespace utils
{
    bool try_other_layouts(juce::AudioProcessor::Bus* bus, juce::AudioChannelSet& requested)
    {
        std::vector<juce::AudioChannelSet> candidates;

        switch (requested.size())
        {
            case 1:
            case 2:
                candidates = {
                    juce::AudioChannelSet::mono(),
                    juce::AudioChannelSet::stereo()
                };
                break;
            case 4:
                candidates = {
                    juce::AudioChannelSet::quadraphonic(),
                    juce::AudioChannelSet::create5point0(),
                    juce::AudioChannelSet::stereo(),
                    juce::AudioChannelSet::mono()
                };
                break;
            case 6:
                candidates = {
                    juce::AudioChannelSet::create5point1(),
                    juce::AudioChannelSet::create5point0(),
                    juce::AudioChannelSet::quadraphonic(),
                    juce::AudioChannelSet::stereo(),
                    juce::AudioChannelSet::mono()
                };
                break;
            case 8:
                candidates = {
                    juce::AudioChannelSet::create7point1(),
                    juce::AudioChannelSet::create5point1(),
                    juce::AudioChannelSet::stereo(),
                    juce::AudioChannelSet::mono()
                };
                break;
        }

        for (const auto& candidate : candidates)
        {
            auto success = bus->setCurrentLayout(candidate);
            if (success) return true;
        }
        return false;
    }

    bool areDuplicates(const juce::PluginDescription& first, const juce::PluginDescription& second)
    {
        const auto tie = [](const juce::PluginDescription& desc) {
            return std::tie(desc.name, desc.manufacturerName, desc.deprecatedUid, desc.uniqueId);
        };

        return tie(first) == tie(second);
    }
}