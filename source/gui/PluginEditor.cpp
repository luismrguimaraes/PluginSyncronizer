#include "PluginEditor.h"
#include "../utils.hpp"
#include "MyTableListBoxModel.hpp"
#include "juce_core/system/juce_PlatformDefs.h"
#include <algorithm>
#include <memory>

namespace
{
    constexpr auto kButtonHeight = 30;
    constexpr auto kMargin = 10;

    constexpr auto kEditorHeight { 500 };
    constexpr auto kEditorWidth { 750 };

    constexpr int kUIUpdateIntervalMs { 22 };
}

namespace gui
{
    class PluginLoaderComponent final : public juce::Component, public MyTableListBoxModelDelegate
    {
    public:
        template <typename Callback>
        PluginLoaderComponent(juce::AudioPluginFormatManager& manager,
            juce::KnownPluginList& list,
            juce::File logs_directory,
            Callback&& callback)
            : plugin_list_component_(manager, list, logs_directory.getChildFile("deadMansPedal.txt"), {})
        {
            tlm_ = new MyTableListBoxModel { plugin_list_component_, list, this };
            plugin_list_component_.setTableModel(std::move(tlm_));
            tlm_ = nullptr;

            plugin_list_component_.getTableListBox().setMultipleSelectionEnabled(false);
            addAndMakeVisible(plugin_list_component_);

            callback_ = [&list, cb = std::forward<Callback>(callback)](int rowNumber, EditorStyle style) {
                const auto& types = list.getTypes();

                if (juce::isPositiveAndBelow(rowNumber, types.size()))
                    juce::NullCheckedInvocation::invoke(cb, types.getReference(rowNumber), style);
            };
        }

        void resized() override
        {
            juce::Grid grid;
            grid.setGap(juce::Grid::Px { kMargin });
            grid.templateColumns = { juce::Grid::TrackInfo { juce::Grid::Fr { 1 } } };
            grid.templateRows = { juce::Grid::TrackInfo { juce::Grid::Fr { 1 } } };
            grid.items = { juce::GridItem { plugin_list_component_ } };
            grid.performLayout(getLocalBounds());
        }

        void openPluginAt(int rowNumber) override
        {
            callback_(rowNumber, EditorStyle::newWindow);
        }

    private:
        juce::PluginListComponent plugin_list_component_;
        MyTableListBoxModel* tlm_;

        std::function<void(int, EditorStyle)> callback_;
    };

    class PluginEditorComponent final : public juce::Component
    {
    public:
        PluginEditorComponent(std::unique_ptr<juce::AudioProcessorEditor> editorIn)
            : editor_(std::move(editorIn))
        {
            addAndMakeVisible(editor_.get());
            childBoundsChanged(editor_.get());
        }

        void setScaleFactor(float scale)
        {
            if (editor_ != nullptr)
                editor_->setScaleFactor(scale);
        }

        void resized() override
        {
            editor_->setSize(getWidth(), getHeight());
        }

        void childBoundsChanged(Component* child) override
        {
            if (child != editor_.get())
                return;

            const auto size = editor_ != nullptr ? editor_->getLocalBounds()
                                                 : juce::Rectangle<int>();

            setSize(size.getWidth(), size.getHeight());
        }

    private:
        std::unique_ptr<juce::AudioProcessorEditor> editor_;
    };

    class ScaledDocumentWindow final : public juce::DocumentWindow
    {
    public:
        ScaledDocumentWindow(juce::Colour bg, float scale, juce::String name, ScaledDocumentWindowDelegate* delegate)
            : DocumentWindow(name, bg, 4), delegate_ { std::move(delegate) }, desktopScale(scale) {}

        float getDesktopScaleFactor() const override { return juce::Desktop::getInstance().getGlobalScaleFactor() * desktopScale; }

        void closeButtonPressed() override
        {
            delegate_->callbackWindowClosed();
        }

        void userTriedToCloseWindow() override
        {
            delegate_->callbackWindowClosed();
        }

        void windowControlClickedClose() override
        {
            delegate_->callbackWindowClosed();
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey)
            {
                delegate_->callbackWindowClosed();
            }
            return true;
        }

    private:
        ScaledDocumentWindowDelegate* delegate_ { nullptr };

        float desktopScale = 1.0f;
    };

    PluginEditor::PluginEditor(PluginProcessor& p, juce::File logs_directory)
        : AudioProcessorEditor { &p }, processor_ref_ { p }, scoped_callback_ { p.plugin_changed_, [this] { pluginChanged(); } }, ui_update_timer_ { [this]() { updateUI(); } }
    {
        loader_ = std::make_unique<PluginLoaderComponent>(p.plugin_format_manager_, p.plugin_list_, logs_directory, [&p](const juce::PluginDescription& pd, EditorStyle editorStyle) {
            p.createSharedPluginInstance(pd, editorStyle);
        });

        setSize(kEditorWidth, kEditorHeight);
        setResizable(true, true);

        addAndMakeVisible(main_component_);

        main_component_.addChildComponent(remove_button_);
        main_component_.addChildComponent(show_button_);
        main_component_.addChildComponent(propagate_state_button_);
        main_component_.addChildComponent(accept_changes_toggle_);
        main_component_.addChildComponent(propagate_changes_toggle_);
        main_component_.addChildComponent(loader_.get());

        inner_name_.setJustificationType(juce::Justification::centred);
        main_component_.addChildComponent(inner_name_);

        pluginChanged();

        remove_button_.onClick = [this] { clearPlugin(); };
        show_button_.onClick = [this] {
            showInnerEditor(true);
        };
        propagate_state_button_.onClick = [this] { processor_ref_.propagateState(); };

        accept_changes_toggle_.setToggleState(processor_ref_.getAcceptChanges(), juce::NotificationType::dontSendNotification);
        accept_changes_toggle_.onClick = [this] {
            auto old_value = processor_ref_.getAcceptChanges();
            processor_ref_.setAcceptChanges(!old_value);
            accept_changes_toggle_.setToggleState(!old_value, juce::NotificationType::sendNotification);
        };
        propagate_changes_toggle_.setToggleState(processor_ref_.getPropagateChanges(), juce::NotificationType::dontSendNotification);
        propagate_changes_toggle_.onClick = [this] {
            auto old_value = processor_ref_.getPropagateChanges();
            processor_ref_.setPropagateChanges(!old_value);
            propagate_changes_toggle_.setToggleState(!old_value, juce::NotificationType::sendNotification);
        };

#ifdef DEBUG
        // addAndMakeVisible(inspect_button_);
        inspect_button_.onClick = [&] {
            if (!inspector_)
            {
                inspector_ = std::make_unique<melatonin::Inspector>(*this);
                inspector_->onClose = [this]() { inspector_.reset(); };
            }

            inspector_->setVisible(true);
        };
#endif

        // setBounds
        {
            main_component_.setBounds({ 0, 0, kEditorWidth, kEditorHeight });

            auto area_if_centered = getLocalBounds().withSizeKeepingCentre(200, kButtonHeight);

            remove_button_.setBounds(area_if_centered.withY(area_if_centered.getY() - kButtonHeight));

            auto remove_button_bounds = remove_button_.getBounds();
            inner_name_.setBounds(remove_button_bounds.withY(remove_button_bounds.getY() - 2 * kButtonHeight));
            inner_name_.setCentrePosition(inner_name_.getBounds().getCentre());

            show_button_.setBounds(area_if_centered.withY(area_if_centered.getY() + kButtonHeight));
            propagate_state_button_.setBounds(area_if_centered.withY(area_if_centered.getY() + 2 * kButtonHeight));

            accept_changes_toggle_.setBounds(remove_button_.getBounds().withX(0));
            propagate_changes_toggle_.setBounds(show_button_.getBounds().withX(0));

            loader_->setBounds(getLocalBounds());
        }

        updateUI();
        ui_update_timer_.startTimer(kUIUpdateIntervalMs);
    }

    PluginEditor::~PluginEditor()
    {
    }

    void PluginEditor::paint(juce::Graphics& g)
    {
        // (Our component is opaque, so we must completely fill the background with a solid colour)
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }

    void PluginEditor::resized()
    {
        auto bounds = getBounds();
        float new_width = bounds.getWidth();
        float new_height = bounds.getHeight();
        float scale = std::min(new_width / static_cast<float>(kEditorWidth), new_height / static_cast<float>(kEditorHeight));
        scale = std::clamp(scale, 0.5f, 4.0f);
        // const std::vector<float> scale_values { 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 2.75, 3.0, 3.25, 3.5, 3.75, 4.0 };
        // auto bounded_scale = std::lower_bound(scale_values.begin(), scale_values.end(), scale);
        // scale = bounded_scale != scale_values.end() ? *bounded_scale : 1.0;
        main_component_.setTransform(juce::AffineTransform::scale(scale));

        // processor_ref_.setUserScale()

        setSize(kEditorWidth * scale, kEditorHeight * scale);
    }

    void PluginEditor::childBoundsChanged([[maybe_unused]] Component* child)
    {
        [[maybe_unused]] const auto child_bounds = inner_editor_window_ != nullptr ? inner_editor_window_->getLocalBounds()
                                                                                   : juce::Rectangle<int>();
    }

    void PluginEditor::setScaleFactor(float scale)
    {
        host_scale_factor_ = scale;
        AudioProcessorEditor::setScaleFactor(scale);

        [[maybe_unused]] const auto posted = juce::MessageManager::callAsync([ref = SafePointer<PluginEditor>(this), scale] {
            if (auto* r = ref.getComponent())
                if (auto* e = r->inner_editor_component_)
                    e->setScaleFactor(scale);
        });

        jassert(posted);
    }

    void PluginEditor::callbackWindowClosed()
    {
        inner_editor_component_ = nullptr;
        inner_editor_window_ = nullptr;
    }

    void PluginEditor::pluginChanged()
    {
        updateComponentsVisibility();

        if (processor_ref_.needsEditorDeletion())
        {
            inner_editor_component_ = nullptr;
            inner_editor_window_ = nullptr;
            return;
        }

        auto pd_opt = processor_ref_.getPluginDescription();
        juce::String new_text = pd_opt.has_value() ? pd_opt->name + " " + pd_opt->pluginFormatName + " (" + pd_opt->manufacturerName + ")" : "";
        inner_name_.setText(new_text, juce::NotificationType::dontSendNotification);

        if (processor_ref_.wantsInnerEditor())
        {
            showInnerEditor(false);
        }
    }

    void PluginEditor::clearPlugin()
    {
        processor_ref_.removeInner();
        updateUI();
        loader_->grabKeyboardFocus();
    }

    void PluginEditor::updateUI()
    {
        updateComponentsVisibility();
    }

    void PluginEditor::updateComponentsVisibility()
    {
        auto is_loaded = processor_ref_.isAnInnerLoaded();

        inner_name_.setVisible(is_loaded);
        remove_button_.setVisible(is_loaded);
        show_button_.setVisible(is_loaded);
        accept_changes_toggle_.setVisible(is_loaded);
        propagate_changes_toggle_.setVisible(is_loaded);
        propagate_state_button_.setVisible(is_loaded);

        loader_->setVisible(!is_loaded);
    }

    void PluginEditor::showInnerEditor(bool wants_keyboard_focus)
    {
        if (inner_editor_window_)
        {
            inner_editor_window_->grabKeyboardFocus();
            return;
        }

        auto inner_editor = processor_ref_.createInnerEditor();
        auto make_inner_editor_resizable = inner_editor->resizableCorner == nullptr && inner_editor->isResizable();
        auto editorComponent = std::make_unique<PluginEditorComponent>(std::move(inner_editor));
        inner_editor_component_ = editorComponent.get();
        inner_editor_component_->setScaleFactor(host_scale_factor_);

        inner_editor_window_ = [&]() -> std::unique_ptr<Component> {
            switch (processor_ref_.getEditorStyle())
            {
                case EditorStyle::thisWindow:
                    require(false); // not implemented
                    addChildComponent(editorComponent.get());
                    setSize(editorComponent->getWidth(), editorComponent->getHeight());
                    return std::move(editorComponent);

                case EditorStyle::newWindow:
                    const auto bg = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker();
                    auto window = std::make_unique<ScaledDocumentWindow>(bg, host_scale_factor_, inner_name_.getText(), this);
                    window->setAlwaysOnTop(true);
                    window->setContentOwned(editorComponent.release(), true);
                    window->centreAroundComponent(this, window->getWidth(), window->getHeight());
                    window->setResizable(make_inner_editor_resizable, false);
#ifdef JUCE_MAC
                    // on macOS "setUsingNativeTitleBar" makes window dragging smoother but misses the initial window size so we compensate
                    window->setUsingNativeTitleBar(true);
                    window->setSize(window->getWidth() - 1, window->getHeight() - 27);
#endif
                    return window;
            }

            jassertfalse;
            return nullptr;
        }();

        inner_editor_window_->setVisible(true);
        if (!wants_keyboard_focus)
        {
            inner_editor_window_->giveAwayKeyboardFocus();
            if (this->isShowing())
            {
                this->grabKeyboardFocus();
            }
        }
    }
}