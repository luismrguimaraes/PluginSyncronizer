#include <juce_audio_processors/juce_audio_processors.h>
#include <string>

namespace utils
{
    inline const std::string kMainStateTag { "state" };
    inline const std::string kSharedStateTag { "shared_state" };

    bool
        try_other_layouts(juce::AudioProcessor::Bus* bus, juce::AudioChannelSet& requested);
}
