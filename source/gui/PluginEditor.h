#pragma once

#include "../PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"

namespace gui
{
    class PluginEditorComponent;
    class PluginLoaderComponent;

    class ScaledDocumentWindowDelegate
    {
    public:
        virtual void callbackWindowClosed() = 0;
    };

    class PluginEditor : public ScaledDocumentWindowDelegate, public juce::AudioProcessorEditor
    {
    public:
        explicit PluginEditor(PluginProcessor&);
        ~PluginEditor() override;

        void paint(juce::Graphics&) override;
        void resized() override;
        void childBoundsChanged(Component* child) override;
        void setScaleFactor(float scale) override;

        // ScaledDocumentWindowDelegate
        void callbackWindowClosed() override;

    private: //NOLINT
        void pluginChanged();
        void clearPlugin();

        void updateUI();
        void updateComponentsVisibility();

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)

        PluginProcessor& processor_ref_;

        std::unique_ptr<PluginLoaderComponent> loader_;
        std::unique_ptr<melatonin::Inspector> inspector_;
        juce::ScopedValueSetter<std::function<void()>> scoped_callback_;

        std::unique_ptr<Component> inner_editor_window_;
        PluginEditorComponent* inner_editor_component_ = nullptr;

        juce::TimedCallback ui_update_timer_;

        float current_scale_factor_ { 1.0f };

        juce::TextButton close_button_ { "Close Plugin" };
        juce::TextButton show_button_ { "Show Plugin UI" };
        juce::TextButton propagate_state_button_ { "Propagate State" };
        juce::TextButton inspect_button_ { "Inspect the UI" };

        juce::ToggleButton accept_changes_toggle_ { "Accept Changes" };
        juce::ToggleButton propagate_changes_toggle_ { "Propagate Changes" };
    };
}