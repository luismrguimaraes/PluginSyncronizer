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
    void setNewPlugin (const juce::PluginDescription& pd, EditorStyle where, const juce::MemoryBlock& mb = {});

    void clearPlugin()
    {
        const juce::ScopedLock sl (innerMutex_);

        inner_ = nullptr;
        juce::NullCheckedInvocation::invoke (pluginChanged_);
    }

    bool isPluginLoaded() const
    {
        const juce::ScopedLock sl (innerMutex_);
        return inner_ != nullptr;
    }

    std::unique_ptr<juce::AudioProcessorEditor> createInnerEditor() const
    {
        const juce::ScopedLock sl (innerMutex_);
        return rawToUniquePtr (inner_->hasEditor() ? inner_->createEditorIfNeeded() : nullptr);
    }

    EditorStyle getEditorStyle() const noexcept
    {
        return editorStyle_;
    }

    // juce::AudioProcessorListener
    void audioProcessorParameterChanged (AudioProcessor* processor,
        int parameterIndex,
        float newValue) override;
    void audioProcessorChanged (AudioProcessor* processor, const ChangeDetails& details) override;

    void loadInnerState (const juce::PluginDescription& plugin_description, const juce::MemoryBlock& state_memory_block) override;

public:
    juce::ApplicationProperties appProperties_;
    juce::AudioPluginFormatManager pluginFormatManager_;
    juce::KnownPluginList pluginList_;
    std::function<void()> pluginChanged_;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)

    juce::CriticalSection innerMutex_;
    std::unique_ptr<juce::AudioPluginInstance> inner_;
    juce::PluginDescription inner_plugin_description_;
    EditorStyle editorStyle_ = EditorStyle {};
    bool active_ = false;
    juce::ScopedMessageBox messageBox_;

    juce::AudioBuffer<float> float_process_buffer_;
    juce::AudioProcessorValueTreeState apvts_;

    int group_ { 0 };

    SharedDataManager shared_data_manager_;
};
