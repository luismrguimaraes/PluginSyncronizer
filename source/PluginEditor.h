#pragma once

#include "BinaryData.h"
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
    static constexpr auto buttonHeight = 30;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;

    std::unique_ptr<PluginLoaderComponent> loader;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::ScopedValueSetter<std::function<void()>> scopedCallback;
    std::unique_ptr<Component> editor;

    float currentScaleFactor { 1.0f };

    juce::TextButton closeButton { "Close Plugin" };
    juce::TextButton inspectButton { "Inspect the UI" };

    PluginEditorComponent* currentEditorComponent = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
