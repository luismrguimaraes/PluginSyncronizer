#include <juce_audio_processors/juce_audio_processors.h>
#include <string>

namespace utils
{
    inline const juce::String kMainStateTag { "state" };
    inline const juce::String kSharedStateTag { "shared_state" };

    bool
        try_other_layouts(juce::AudioProcessor::Bus* bus, juce::AudioChannelSet& requested);

#define require(expression) \
    if (!(expression)) std::terminate;
}
