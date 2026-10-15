#pragma once

#include "../PluginProcessor.hpp"
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
        PluginEditor(PluginProcessor& processor_ref, juce::File logs_directory);
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

        void showInnerEditor(bool wants_keyboard_focus);

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)

        PluginProcessor& processor_ref_;

        std::unique_ptr<PluginLoaderComponent> loader_;
        std::unique_ptr<melatonin::Inspector> inspector_;
        juce::ScopedValueSetter<std::function<void()>> scoped_callback_;

        std::unique_ptr<Component> inner_editor_window_;
        PluginEditorComponent* inner_editor_component_ = nullptr;

        juce::TimedCallback ui_update_timer_;

        float host_scale_factor_ { 1.0f };

        juce::TextButton remove_button_ { "Remove" };
        juce::TextButton show_button_ { "Show UI" };
        juce::TextButton propagate_state_button_ { "Propagate State" };
#ifdef DEBUG
        juce::TextButton inspect_button_ { "Inspect the UI" };
#endif

        juce::ToggleButton accept_changes_toggle_ { "Receive All Changes" };
        juce::ToggleButton propagate_changes_toggle_ { "Send Parameter Changes" };

        juce::Label inner_name_;
        juce::Label supports_sidechain_;
        juce::Label version_info_;
        juce::Label no_scanned_plugins_;
        juce::Label click_here_to_scan_;

        juce::LookAndFeel_V4 look_and_feel_;

        juce::Component main_component_; // this allows rescaling the whole plugin with a juce::AffineTransform
    };
}