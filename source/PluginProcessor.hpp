#pragma once

#include "SharedData.hpp"
#include "debug/JuceUtils.hpp"
#include "farbot/include/farbot/AsyncCaller.hpp"
#include "farbot/include/farbot/RealtimeObject.hpp"
#include "farbot/include/farbot/fifo.hpp"
#include "juce_core/system/juce_PlatformDefs.h"
#include "utils.hpp"
#include <exception>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

#if (MSVC)
    #include "ipps.h"
#endif

enum class EditorStyle {
    thisWindow,
    newWindow
};

struct EditorProps
{
    void getStateInformation(juce::MemoryBlock& destData, juce::File logs_directory)
    {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

        juce::XmlElement xml { utils::kEditorPropsTag };
        xml.setAttribute("user_scale", user_scale);

        xml.writeTo(logs_directory.getChildFile("EditorPropsState.xml"));

        const auto xlm_as_string = xml.toString();
        destData.replaceAll(xlm_as_string.toRawUTF8(), xlm_as_string.getNumBytesAsUTF8());
    }

    void setStateInformation(const void* data, int sizeInBytes)
    {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

        auto xml = juce::XmlDocument::parse(juce::String(juce::CharPointer_UTF8(static_cast<const char*>(data)), static_cast<size_t>(sizeInBytes)));
        require(xml != nullptr);

        auto recovered_user_scale = xml->getDoubleAttribute("user_scale", -1.0);
        require(recovered_user_scale > 0);
        user_scale = static_cast<float>(recovered_user_scale);
    }

    float user_scale { 1.0 };
};

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

    void createSharedPluginInstance(const juce::PluginDescription& pd, EditorStyle where = EditorStyle::newWindow);
    std::optional<juce::PluginDescription> getPluginDescription() const;

    void removeInner();
    bool needsEditorDeletion() const;
    bool isAnInnerLoaded() const;
    bool wantsInnerEditor() const;
    void propagateState();
    bool innerSupportsSidechain() const;

    bool getAcceptChanges() const;
    void setAcceptChanges(bool accept);
    bool getPropagateChanges() const;
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

    float getUserScale();
    void setUserScale(float user_scale);

    juce::KnownPluginList& getPluginList() noexcept;

    // For Tests
    void setInnerParameter(int parameterIndex, float newValue);
    float getInnerParameter(int parameterIndex) const;
    juce::AudioProcessor* getInner(); // not const, but changing this will not change the NonRTMutatable

public:
    juce::AudioPluginFormatManager plugin_format_manager_;
    std::function<void()> plugin_changed_;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void setNewPlugin(const juce::PluginDescription& pd, const juce::MemoryBlock& mb, std::optional<std::function<void()>> onSuccessCallback = std::nullopt);
    bool prepareInstance(juce::AudioPluginInstance* instance, double sampleRate, int samplesPerBlock);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)

    juce::ApplicationProperties app_properties_;
    juce::File logs_directory_;
    juce::KnownPluginList plugin_list_;

    using NRTMAudioPluginInstance = farbot::RealtimeObject<std::shared_ptr<juce::AudioPluginInstance>, farbot::RealtimeObjectOptions::nonRealtimeMutatable>;
    mutable NRTMAudioPluginInstance inner_;
    std::optional<juce::PluginDescription> inner_plugin_description_;
    juce::ScopedMessageBox message_box_;

    bool is_an_inner_loaded_ { false };
    bool needs_editor_deletion_ { false };
    bool wants_inner_editor_ { false };

    std::atomic<bool> inner_prepared_ { false };
    std::atomic<bool> are_we_playing_ { false };

    juce::AudioBuffer<float> inner_process_buffer_;
    juce::AudioBuffer<float> double_process_buffer_;
    juce::AudioProcessorValueTreeState apvts_;

    int group_ { 0 };

    SharedDataManager shared_data_manager_;
    bool accept_changes_ { true };
    std::atomic<bool> propagate_changes_ { true };

    farbot::AsyncCaller<farbot::fifo_options::concurrency::multiple> async_caller_;
    juce::TimedCallback async_call_processing_timer_;

    EditorProps editor_props_;

#ifdef DEBUG
    JuceDebugger juce_debugger_;
#endif
};
