#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

class JuceDebugger
{
public:
    void prepareToPlay(int samplesPerBlock, int inputChannels, int outputChannels);
    void readInput(const juce::AudioBuffer<float>& buffer);
    void readOutput(const juce::AudioBuffer<float>& buffer);

private:
    std::vector<std::vector<float>> juce_input_buffer_debugger_;
    std::vector<std::vector<float>> juce_output_buffer_debugger_;
};

void fill_with_silence(juce::AudioBuffer<float>& buffer_to_fill);
bool is_silence(juce::AudioBuffer<float>& buffer);
