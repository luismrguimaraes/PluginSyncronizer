#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

class JuceDebugger
{
public:
    void prepareToPlay(int samplesPerBlock, int inputChannels, int outputChannels);
    void processInput(const juce::AudioBuffer<float>& buffer);
    void processOutput(const juce::AudioBuffer<float>& buffer);

private:
    std::vector<std::vector<float>> juce_input_buffer_debugger_;
    std::vector<std::vector<float>> juce_output_buffer_debugger_;
};