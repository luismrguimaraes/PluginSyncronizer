#pragma once

#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"

class PluginLoaderComponent;
class PluginEditorComponent;
//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void childBoundsChanged (Component* child) override;
    void setScaleFactor (float scale) override;

private: //NOLINT
    void pluginChanged();
    void clearPlugin();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef_;

    std::unique_ptr<PluginLoaderComponent> loader_;
    std::unique_ptr<melatonin::Inspector> inspector_;
    juce::ScopedValueSetter<std::function<void()>> scopedCallback_;
    std::unique_ptr<Component> editor_;

    float currentScaleFactor_ { 1.0f };

    juce::TextButton closeButton_ { "Close Plugin" };
    juce::TextButton showButton_ { "Show Plugin" };
    juce::TextButton syncButton_ { "Sync" };
    juce::TextButton inspectButton_ { "Inspect the UI" };

    PluginEditorComponent* innerEditorComponent_ = nullptr;
};
