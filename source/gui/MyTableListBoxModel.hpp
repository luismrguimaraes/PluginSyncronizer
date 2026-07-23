#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_audio_processors_headless/juce_audio_processors_headless.h"
#include "juce_gui_basics/juce_gui_basics.h"

namespace gui
{
    class MyTableListBoxModelDelegate
    {
    public:
        virtual void openPluginAt(int rowNumber) = 0;
    };

    class MyTableListBoxModel : public juce::TableListBoxModel
    {
    public:
        MyTableListBoxModel(juce::PluginListComponent& c, juce::KnownPluginList& l, MyTableListBoxModelDelegate* delegate);

        int getNumRows() override;

        void paintRowBackground(juce::Graphics& g, int /*rowNumber*/, int /*width*/, int /*height*/, bool rowIsSelected) override;

        enum {
            nameCol = 1,
            typeCol = 2,
            categoryCol = 3,
            manufacturerCol = 4,
            descCol = 5
        };

        void paintCell(juce::Graphics& g, int row, int columnId, int width, int height, bool /*rowIsSelected*/) override;

        void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;

        void cellDoubleClicked(int rowNumber, [[maybe_unused]] int columnId, const juce::MouseEvent&) override;

        void returnKeyPressed(int lastRowSelected) override;

        void deleteKeyPressed(int) override;

        void sortOrderChanged(int newSortColumnId, bool isForwards) override;

        static juce::String getPluginDescription(const juce::PluginDescription& desc);

    private:
        juce::PluginListComponent& owner;
        juce::KnownPluginList& list;

        MyTableListBoxModelDelegate* delegate_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyTableListBoxModel)
    };
}