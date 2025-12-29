#include "PluginEditor.h"

namespace
{
    constexpr auto kButtonHeight = 30;
    constexpr auto kMargin = 10;

    constexpr auto kEditorHeight { 500 };
    constexpr auto kEditorWidth { 750 };
}

static void doLayout (juce::Component* main, juce::Component& bottom, int bottomHeight, juce::Rectangle<int> bounds)
{
    juce::Grid grid;
    grid.setGap (juce::Grid::Px { kMargin });
    grid.templateColumns = { juce::Grid::TrackInfo { juce::Grid::Fr { 1 } } };
    grid.templateRows = { juce::Grid::TrackInfo { juce::Grid::Fr { 1 } },
        juce::Grid::TrackInfo { juce::Grid::Px { bottomHeight } } };
    grid.items = { juce::GridItem { main }, juce::GridItem { bottom }.withMargin ({ 0, kMargin, kMargin, kMargin }) };
    grid.performLayout (bounds);
}

class PluginLoaderComponent final : public juce::Component
{
public:
    template <typename Callback>
    PluginLoaderComponent (juce::AudioPluginFormatManager& manager,
        juce::KnownPluginList& list,
        Callback&& callback)
        : pluginListComponent (manager, list, {}, {})
    {
        pluginListComponent.getTableListBox().setMultipleSelectionEnabled (false);

        addAndMakeVisible (pluginListComponent);
        addAndMakeVisible (buttons);

        const auto getCallback = [this, &list, cb = std::forward<Callback> (callback)] (EditorStyle style) {
            return [this, &list, cb, style] {
                const auto index = pluginListComponent.getTableListBox().getSelectedRow();
                const auto& types = list.getTypes();

                if (juce::isPositiveAndBelow (index, types.size()))
                    juce::NullCheckedInvocation::invoke (cb, types.getReference (index), style);
            };
        };

        buttons.thisWindowButton.onClick = getCallback (EditorStyle::thisWindow);
        buttons.newWindowButton.onClick = getCallback (EditorStyle::newWindow);
    }

    void resized() override
    {
        doLayout (&pluginListComponent, buttons, 80, getLocalBounds());
    }

private:
    struct Buttons final : public Component
    {
        Buttons()
        {
            label.setJustificationType (juce::Justification::centred);

            addAndMakeVisible (label);
            // addAndMakeVisible (thisWindowButton);
            addAndMakeVisible (newWindowButton);
        }

        void resized() override
        {
            juce::Grid vertical;
            vertical.autoFlow = juce::Grid::AutoFlow::row;
            vertical.setGap (juce::Grid::Px { kMargin });
            vertical.autoRows = vertical.autoColumns = juce::Grid::TrackInfo { juce::Grid::Fr { 1 } };
            vertical.items.insertMultiple (0, juce::GridItem {}, 2);
            vertical.performLayout (getLocalBounds());

            label.setBounds (vertical.items[0].currentBounds.toNearestInt());

            juce::Grid grid;
            grid.autoFlow = juce::Grid::AutoFlow::column;
            grid.setGap (juce::Grid::Px { kMargin });
            grid.autoRows = grid.autoColumns = juce::Grid::TrackInfo { juce::Grid::Fr { 1 } };
            grid.items = { juce::GridItem { thisWindowButton },
                juce::GridItem { newWindowButton } };

            grid.performLayout (vertical.items[1].currentBounds.toNearestInt());
        }

        juce::Label label { "", "Select a plugin from the list, then display it using the buttons below." };
        juce::TextButton thisWindowButton { "Open In This Window" };
        juce::TextButton newWindowButton { "Open In New Window" };
    };

    juce::PluginListComponent pluginListComponent;
    Buttons buttons;
};

class PluginEditorComponent final : public juce::Component
{
public:
    template <typename Callback>
    PluginEditorComponent (std::unique_ptr<juce::AudioProcessorEditor> editorIn, Callback&& onClose)
        : editor (std::move (editorIn))
    {
        addAndMakeVisible (editor.get());
        // addAndMakeVisible (closeButton);

        childBoundsChanged (editor.get());

        closeButton.onClick = std::forward<Callback> (onClose);
    }

    void setScaleFactor (float scale)
    {
        if (editor != nullptr)
            editor->setScaleFactor (scale);
    }

    void resized() override
    {
        doLayout (editor.get(), closeButton, kButtonHeight, getLocalBounds());
    }

    void childBoundsChanged (Component* child) override
    {
        if (child != editor.get())
            return;

        const auto size = editor != nullptr ? editor->getLocalBounds()
                                            : juce::Rectangle<int>();

        setSize (size.getWidth(), kMargin + 30 + size.getHeight());
    }

private:
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::TextButton closeButton { "Close Plugin" };
};

//==============================================================================
class ScaledDocumentWindow final : public juce::DocumentWindow
{
public:
    ScaledDocumentWindow (juce::Colour bg, float scale)
        : DocumentWindow ("Editor", bg, 4), desktopScale (scale) {}

    float getDesktopScaleFactor() const override { return juce::Desktop::getInstance().getGlobalScaleFactor() * desktopScale; }

    void closeButtonPressed() override { setVisible (false); }

private:
    float desktopScale = 1.0f;
};

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef_ (p), scopedCallback_ (p.pluginChanged_, [this] { pluginChanged(); })
{
    loader_ = std::make_unique<PluginLoaderComponent> (p.pluginFormatManager_, p.pluginList_, [&p] (const juce::PluginDescription& pd, EditorStyle editorStyle) {
        p.setNewPlugin (pd, editorStyle);
    });

    setSize (kEditorWidth, kEditorHeight);
    setResizable (false, false);
    addAndMakeVisible (closeButton_);
    addAndMakeVisible (showButton_);
    addAndMakeVisible (loader_.get());

    p.pluginChanged_();

    closeButton_.onClick = [this] { clearPlugin(); };
    showButton_.onClick = [this] { editor_->setVisible (true); };

    addAndMakeVisible (inspectButton_);

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton_.onClick = [&] {
        if (!inspector_)
        {
            inspector_ = std::make_unique<melatonin::Inspector> (*this);
            inspector_->onClose = [this]() { inspector_.reset(); };
        }

        inspector_->setVisible (true);
    };
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // auto area = getLocalBounds();
    // g.setColour (juce::Colours::white);
    // g.setFont (16.0f);
    // auto helloWorld = juce::String ("Hello from ") + PRODUCT_NAME_WITHOUT_VERSION + " v" VERSION + " running in " + CMAKE_BUILD_TYPE;
    // g.drawText (helloWorld, area.removeFromTop (150), juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    // // layout the positions of your child components here
    // auto area = getLocalBounds();
    // area.removeFromBottom (50);
    // inspectButton.setBounds (getLocalBounds().withSizeKeepingCentre (100, 50));

    auto area_if_centered = getLocalBounds().withSizeKeepingCentre (200, kButtonHeight);
    closeButton_.setBounds (area_if_centered.withY (area_if_centered.getY() - kButtonHeight));
    showButton_.setBounds (area_if_centered.withY (area_if_centered.getY() + kButtonHeight));
    loader_->setBounds (getLocalBounds());
}

void PluginEditor::childBoundsChanged ([[maybe_unused]] Component* child)
{
    const auto bounds = editor_ != nullptr ? editor_->getLocalBounds()
                                           : juce::Rectangle<int>();

    setSize (bounds.getWidth(), bounds.getHeight());
}

void PluginEditor::setScaleFactor (float scale)
{
    currentScaleFactor_ = scale;
    AudioProcessorEditor::setScaleFactor (scale);

    [[maybe_unused]] const auto posted = juce::MessageManager::callAsync ([ref = SafePointer<PluginEditor> (this), scale] {
        if (auto* r = ref.getComponent())
            if (auto* e = r->currentEditorComponent_)
                e->setScaleFactor (scale);
    });

    jassert (posted);
}

void PluginEditor::pluginChanged()
{
    loader_->setVisible (!processorRef_.isPluginLoaded());
    closeButton_.setVisible (processorRef_.isPluginLoaded());
    showButton_.setVisible (processorRef_.isPluginLoaded());

    if (processorRef_.isPluginLoaded())
    {
        auto editorComponent = std::make_unique<PluginEditorComponent> (processorRef_.createInnerEditor(), [this] {
            [[maybe_unused]] const auto posted = juce::MessageManager::callAsync ([this] { clearPlugin(); });
            jassert (posted);
        });

        editorComponent->setScaleFactor (currentScaleFactor_);
        currentEditorComponent_ = editorComponent.get();

        editor_ = [&]() -> std::unique_ptr<Component> {
            switch (processorRef_.getEditorStyle())
            {
                case EditorStyle::thisWindow:
                    addAndMakeVisible (editorComponent.get());
                    setSize (editorComponent->getWidth(), editorComponent->getHeight());
                    return std::move (editorComponent);

                case EditorStyle::newWindow:
                    const auto bg = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker();
                    auto window = std::make_unique<ScaledDocumentWindow> (bg, currentScaleFactor_);
                    window->setAlwaysOnTop (true);
                    window->setContentOwned (editorComponent.release(), true);
                    window->centreAroundComponent (this, window->getWidth(), window->getHeight());
                    window->setVisible (true);
                    window->setUsingNativeTitleBar (true);
                    return window;
            }

            jassertfalse;
            return nullptr;
        }();
    }
    else
    {
        editor_ = nullptr;
        setSize (kEditorWidth, kEditorHeight);
    }
}

void PluginEditor::clearPlugin()
{
    currentEditorComponent_ = nullptr;
    editor_ = nullptr;
    processorRef_.removeInner();
}