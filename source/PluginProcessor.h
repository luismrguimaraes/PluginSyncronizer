#pragma once

#include "SharedData.hpp"
#include <juce_audio_processors/juce_audio_processors.h>

#if (MSVC)
    #include "ipps.h"
#endif

enum class EditorStyle { thisWindow,
    newWindow };

class PluginProcessor : public juce::AudioProcessor, public SharedData::Listener, private juce::ChangeListener, private juce::AudioProcessorListener
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void setNewPlugin (const juce::PluginDescription& pd, const juce::MemoryBlock& mb);
    void createSharedPluginInstance (const juce::PluginDescription& pd, EditorStyle where, const juce::MemoryBlock& mb = {});

    void removeInner() override;

    bool isPluginLoaded() const;

    std::unique_ptr<juce::AudioProcessorEditor> createInnerEditor() const;

    EditorStyle getEditorStyle() const noexcept;

    // juce::AudioProcessorListener
    void audioProcessorParameterChanged (AudioProcessor* processor,
        int parameterIndex,
        float newValue) override;
    void audioProcessorChanged (AudioProcessor* processor, const juce::AudioProcessorListener::ChangeDetails& details) override;

    // SharedData::Listener
    void loadInner (const juce::PluginDescription& pluginDescription, const juce::MemoryBlock& stateMemoryBlock) override;
    void loadInnerState (const juce::PluginDescription& pluginDescription, const juce::MemoryBlock& stateMemoryBlock) override;
    void updateParameterValue (int parameterIndex, float newValue) override;

    void syncInners();

public:
    juce::ApplicationProperties app_properties_;
    juce::AudioPluginFormatManager plugin_format_manager_;
    juce::KnownPluginList plugin_list_;
    std::function<void()> plugin_changed_;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)

    juce::CriticalSection inner_mutex_;
    std::unique_ptr<juce::AudioPluginInstance> inner_;
    std::optional<juce::PluginDescription> inner_plugin_description_;
    EditorStyle editor_style_ = EditorStyle {};
    bool active_ { false };
    juce::ScopedMessageBox message_box_;

    juce::AudioBuffer<float> float_process_buffer_;
    juce::AudioProcessorValueTreeState apvts_;

    int group_ { 0 };

    SharedDataManager shared_data_manager_;
    bool syncing_params_ { true };
};
