#include "PluginEditor.h"
#include "MyTableListBoxModel.hpp"
#include "juce_core/system/juce_PlatformDefs.h"
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
            Callback&& callback)
            : pluginListComponent_(manager, list, juce::File { "~/Desktop/PluginHostDev/deadMansFile.txt" }, {})
        {
            tlm_ = new MyTableListBoxModel { pluginListComponent_, list, this };
            pluginListComponent_.setTableModel(std::move(tlm_));
            tlm_ = nullptr;

            pluginListComponent_.getTableListBox().setMultipleSelectionEnabled(false);
            addAndMakeVisible(pluginListComponent_);

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
            grid.items = { juce::GridItem { pluginListComponent_ } };
            grid.performLayout(getLocalBounds());
        }

        void openPluginAt(int rowNumber) override
        {
            callback_(rowNumber, EditorStyle::newWindow);
        }

    private:
        juce::PluginListComponent pluginListComponent_;
        MyTableListBoxModel* tlm_;

        std::function<void(int, EditorStyle)> callback_;
    };

    class PluginEditorComponent final : public juce::Component
    {
    public:
        template <typename Callback>
        PluginEditorComponent(std::unique_ptr<juce::AudioProcessorEditor> editorIn, Callback&& onClose)
            : editor(std::move(editorIn))
        {
            addAndMakeVisible(editor.get());
            childBoundsChanged(editor.get());
        }

        void setScaleFactor(float scale)
        {
            if (editor != nullptr)
                editor->setScaleFactor(scale);
        }

        void resized() override {}

        void childBoundsChanged(Component* child) override
        {
            if (child != editor.get())
                return;

            const auto size = editor != nullptr ? editor->getLocalBounds()
                                                : juce::Rectangle<int>();

            setSize(size.getWidth(), size.getHeight());
        }

    private:
        std::unique_ptr<juce::AudioProcessorEditor> editor;
    };

    class ScaledDocumentWindow final : public juce::DocumentWindow
    {
    public:
        ScaledDocumentWindow(juce::Colour bg, float scale, ScaledDocumentWindowDelegate* delegate)
            : DocumentWindow("Editor", bg, 4), delegate_ { std::move(delegate) }, desktopScale(scale) {}

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

    PluginEditor::PluginEditor(PluginProcessor& p)
        : AudioProcessorEditor(&p), processor_ref_(p), scoped_callback_(p.plugin_changed_, [this] { pluginChanged(); }), ui_update_timer_ { [this]() { updateUI(); } }
    {
        loader_ = std::make_unique<PluginLoaderComponent>(p.plugin_format_manager_, p.plugin_list_, [&p](const juce::PluginDescription& pd, EditorStyle editorStyle) {
            p.createSharedPluginInstance(pd, editorStyle);
        });

        setSize(kEditorWidth, kEditorHeight);
        setResizable(false, false);
        addAndMakeVisible(close_button_);
        addAndMakeVisible(show_button_);
        addAndMakeVisible(propagate_state_button_);
        addAndMakeVisible(accept_changes_toggle_);
        addAndMakeVisible(propagate_changes_toggle_);
        addAndMakeVisible(loader_.get());

        addAndMakeVisible(inner_name_);

        pluginChanged();

        close_button_.onClick = [this] { clearPlugin(); };
        show_button_.onClick = [this] {
            if (inner_editor_window_) return;

            auto editorComponent = std::make_unique<PluginEditorComponent>(processor_ref_.createInnerEditor(), [this] {
                [[maybe_unused]] const auto posted = juce::MessageManager::callAsync([this] { clearPlugin(); });
                jassert(posted);
            });
            inner_editor_component_ = editorComponent.get();
            inner_editor_component_->setScaleFactor(current_scale_factor_);

            inner_editor_window_ = [&]() -> std::unique_ptr<Component> {
                switch (processor_ref_.getEditorStyle())
                {
                    case EditorStyle::thisWindow:
                        addChildComponent(editorComponent.get());
                        setSize(editorComponent->getWidth(), editorComponent->getHeight());
                        return std::move(editorComponent);

                    case EditorStyle::newWindow:
                        const auto bg = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker();
                        auto window = std::make_unique<ScaledDocumentWindow>(bg, current_scale_factor_, this);
                        window->setAlwaysOnTop(true);
                        window->setContentOwned(editorComponent.release(), true);
                        window->centreAroundComponent(this, window->getWidth(), window->getHeight());
                        window->setUsingNativeTitleBar(true);
                        return window;
                }

                jassertfalse;
                return nullptr;
            }();

            inner_editor_window_->setVisible(true);
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

        // addAndMakeVisible(inspect_button_);

        // this chunk of code instantiates and opens the melatonin inspector
        inspect_button_.onClick = [&] {
            if (!inspector_)
            {
                inspector_ = std::make_unique<melatonin::Inspector>(*this);
                inspector_->onClose = [this]() { inspector_.reset(); };
            }

            inspector_->setVisible(true);
        };

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
        auto area_if_centered = getLocalBounds().withSizeKeepingCentre(200, kButtonHeight);

        close_button_.setBounds(area_if_centered.withY(area_if_centered.getY() - kButtonHeight));

        auto close_button_bounds = close_button_.getBounds();
        inner_name_.setBounds(close_button_bounds.withY(close_button_bounds.getY() - 2 * kButtonHeight));
        inner_name_.setCentrePosition(inner_name_.getBounds().getCentre());

        show_button_.setBounds(area_if_centered.withY(area_if_centered.getY() + kButtonHeight));
        propagate_state_button_.setBounds(area_if_centered.withY(area_if_centered.getY() + 2 * kButtonHeight));

        accept_changes_toggle_.setBounds(close_button_.getBounds().withX(0));
        propagate_changes_toggle_.setBounds(show_button_.getBounds().withX(0));

        loader_->setBounds(getLocalBounds());
    }

    void PluginEditor::childBoundsChanged([[maybe_unused]] Component* child)
    {
        const auto bounds = inner_editor_window_ != nullptr ? inner_editor_window_->getLocalBounds()
                                                            : juce::Rectangle<int>();

        setSize(bounds.getWidth(), bounds.getHeight());
    }

    void PluginEditor::setScaleFactor(float scale)
    {
        current_scale_factor_ = scale;
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

        if (processor_ref_.editorNeedsDeletion())
        {
            inner_editor_component_ = nullptr;
            inner_editor_window_ = nullptr;
            setSize(kEditorWidth, kEditorHeight);
            return;
        }

        auto pd_opt = processor_ref_.getPluginDescription();
        if (pd_opt.has_value())
        {
            inner_name_.setText(pd_opt->name + " (" + pd_opt->manufacturerName + ")", juce::NotificationType::dontSendNotification);
        }
        else
        {
            inner_name_.setText("", juce::NotificationType::dontSendNotification);
        }
    }

    void PluginEditor::clearPlugin()
    {
        processor_ref_.removeInner();
    }

    void PluginEditor::updateUI()
    {
        updateComponentsVisibility();
    }

    void PluginEditor::updateComponentsVisibility()
    {
        auto is_loaded = processor_ref_.isPluginLoaded();

        inner_name_.setVisible(is_loaded);
        close_button_.setVisible(is_loaded);
        show_button_.setVisible(is_loaded);
        accept_changes_toggle_.setVisible(is_loaded);
        propagate_changes_toggle_.setVisible(is_loaded);
        propagate_state_button_.setVisible(is_loaded);

        loader_->setVisible(!is_loaded);
    }

}