#pragma once

#include "SharedData.hpp"
#include "debug/DebugUtils.hpp"
#include "farbot/include/farbot/AsyncCaller.hpp"
#include "farbot/include/farbot/RealtimeObject.hpp"
#include "farbot/include/farbot/fifo.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

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

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void createSharedPluginInstance(const juce::PluginDescription& pd, EditorStyle where, const juce::MemoryBlock& mb = {});
    std::optional<juce::PluginDescription> getPluginDescription();

    bool isPluginLoaded() const;
    void removeInner();
    bool editorNeedsDeletion();
    void propagateState();

    bool getAcceptChanges();
    void setAcceptChanges(bool accept);
    bool getPropagateChanges();
    void setPropagateChanges(bool propagate);

    std::unique_ptr<juce::AudioProcessorEditor> createInnerEditor() const;

    EditorStyle getEditorStyle() const noexcept;

    // juce::AudioProcessorListener
    void audioProcessorParameterChanged(AudioProcessor* processor,
        int parameterIndex,
        float newValue) override;
    void audioProcessorChanged(AudioProcessor* processor, const juce::AudioProcessorListener::ChangeDetails& details) override;

    // SharedData::Listener
    void callbackLoadInner(const juce::PluginDescription& pluginDescription) override;
    void callbackLoadInnerState(const juce::PluginDescription& pluginDescription, const juce::MemoryBlock& stateMemoryBlock) override;
    void callbackUpdateParameterValue(int parameterIndex, float newValue) override;
    void callbackRemoveInner() override;

    void notifyHost();

    void numChannelsChanged() override;
    void numBusesChanged() override;
    void processorLayoutsChanged() override;

public:
    juce::ApplicationProperties app_properties_;
    juce::AudioPluginFormatManager plugin_format_manager_;
    juce::KnownPluginList plugin_list_;
    std::function<void()> plugin_changed_;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    bool setNewPlugin(const juce::PluginDescription& pd, const juce::MemoryBlock& mb);
    bool prepareInstance(juce::AudioPluginInstance* instance, double sampleRate, int samplesPerBlock);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)

    using NRTMAudioPluginInstance = farbot::RealtimeObject<std::shared_ptr<juce::AudioPluginInstance>, farbot::RealtimeObjectOptions::nonRealtimeMutatable>;

    mutable NRTMAudioPluginInstance inner_;
    std::optional<juce::PluginDescription> inner_plugin_description_;
    EditorStyle editor_style_ = EditorStyle {};
    juce::ScopedMessageBox message_box_;

    std::atomic<bool> is_a_inner_loaded_ { false };
    std::atomic<bool> editor_needs_deletion_ { false };
    std::atomic<bool> inner_prepared_ { false };
    std::atomic<bool> are_we_playing_ { false };

    juce::AudioBuffer<float> float_process_buffer_;
    juce::AudioProcessorValueTreeState apvts_;

    int group_ { 0 };

    SharedDataManager shared_data_manager_;
    bool accept_changes_ { true };
    std::atomic<bool> propagate_changes_ { true };

    farbot::AsyncCaller<farbot::fifo_options::concurrency::multiple> async_caller_;
    juce::TimedCallback async_call_processing_timer_;

#ifdef DEBUG
    JuceDebugger juce_debugger_;
#endif
};
