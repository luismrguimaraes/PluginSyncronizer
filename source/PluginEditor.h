#pragma once

#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"

class PluginLoaderComponent;
class PluginEditorComponent;
//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void childBoundsChanged(Component* child) override;
    void setScaleFactor(float scale) override;

private: //NOLINT
    void pluginChanged();
    void clearPlugin();

    void updateUI();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)

    // ToDo: fix formatting according to README.md

    PluginProcessor& processorRef_;

    std::unique_ptr<PluginLoaderComponent> loader_;
    std::unique_ptr<melatonin::Inspector> inspector_;
    juce::ScopedValueSetter<std::function<void()>> scopedCallback_;
    std::unique_ptr<Component> editor_;

    juce::TimedCallback ui_update_timer_;

    float currentScaleFactor_ { 1.0f };

    juce::TextButton closeButton_ { "Close Plugin" };
    juce::TextButton showButton_ { "Show Plugin UI" };
    juce::TextButton propagateStateButton_ { "Propagate State" };
    juce::TextButton inspectButton_ { "Inspect the UI" };

    juce::ToggleButton accept_changes_toggle_ { "Accept Changes" };
    juce::ToggleButton propagate_changes_toggle_ { "Propagate Changes" };

    PluginEditorComponent* innerEditorComponent_ = nullptr;
};
