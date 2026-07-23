#include "MyTableListBoxModel.hpp"

namespace gui
{
    MyTableListBoxModel::MyTableListBoxModel(juce::PluginListComponent& c, juce::KnownPluginList& l, MyTableListBoxModelDelegate* delegate) : owner(c), list(l), delegate_ { delegate } {}

    int MyTableListBoxModel::getNumRows()
    {
        return list.getNumTypes() + list.getBlacklistedFiles().size();
    }

    void MyTableListBoxModel::paintRowBackground(juce::Graphics& g, int /*rowNumber*/, int /*width*/, int /*height*/, bool rowIsSelected)
    {
        const auto defaultColour = owner.findColour(juce::ListBox::backgroundColourId);
        const auto c = rowIsSelected ? defaultColour.interpolatedWith(owner.findColour(juce::ListBox::textColourId), 0.5f)
                                     : defaultColour;

        g.fillAll(c);
    }

    enum {
        nameCol = 1,
        typeCol = 2,
        categoryCol = 3,
        manufacturerCol = 4,
        descCol = 5
    };

    void MyTableListBoxModel::paintCell(juce::Graphics& g, int row, int columnId, int width, int height, bool /*rowIsSelected*/)
    {
        juce::String text;
        bool isBlacklisted = row >= list.getNumTypes();

        if (isBlacklisted)
        {
            if (columnId == nameCol)
                text = list.getBlacklistedFiles()[row - list.getNumTypes()];
            else if (columnId == descCol)
                text = TRANS("Deactivated after failing to initialise correctly");
        }
        else
        {
            auto desc = list.getTypes()[row];

            switch (columnId)
            {
                case nameCol:
                    text = desc.name;
                    break;
                case typeCol:
                    text = desc.pluginFormatName;
                    break;
                case categoryCol:
                    text = desc.category.isNotEmpty() ? desc.category : "-";
                    break;
                case manufacturerCol:
                    text = desc.manufacturerName;
                    break;
                case descCol:
                    text = getPluginDescription(desc);
                    break;

                default:
                    jassertfalse;
                    break;
            }
        }

        if (text.isNotEmpty())
        {
            const auto defaultTextColour = owner.findColour(juce::ListBox::textColourId);
            g.setColour(isBlacklisted         ? juce::Colours::red
                        : columnId == nameCol ? defaultTextColour
                                              : defaultTextColour.interpolatedWith(juce::Colours::transparentBlack, 0.3f));
            g.setFont(owner.withDefaultMetrics(juce::FontOptions((float) height * 0.7f, juce::Font::bold)));
            g.drawFittedText(text, 4, 0, width - 6, height, juce::Justification::centredLeft, 1, 0.9f);
        }
    }

    void MyTableListBoxModel::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e)
    {
        juce::TableListBoxModel::cellClicked(rowNumber, columnId, e);

        if (rowNumber >= 0 && rowNumber < getNumRows() && e.mods.isPopupMenu())
        {
            owner.createMenuForRow(rowNumber)
                .showMenuAsync(juce::PopupMenu::Options().withTargetComponent(e.originalComponent).withMousePosition().withDeletionCheck(owner));
        }
    }

    void MyTableListBoxModel::cellDoubleClicked(int rowNumber, [[maybe_unused]] int columnId, const juce::MouseEvent&)
    {
        delegate_->openPluginAt(rowNumber);
    }

    void MyTableListBoxModel::returnKeyPressed(int lastRowSelected)
    {
        delegate_->openPluginAt(lastRowSelected);
    }

    void MyTableListBoxModel::deleteKeyPressed(int)
    {
        owner.removeSelectedPlugins();
    }

    void MyTableListBoxModel::sortOrderChanged(int newSortColumnId, bool isForwards)
    {
        switch (newSortColumnId)
        {
            case nameCol:
                list.sort(juce::KnownPluginList::sortAlphabetically, isForwards);
                break;
            case typeCol:
                list.sort(juce::KnownPluginList::sortByFormat, isForwards);
                break;
            case categoryCol:
                list.sort(juce::KnownPluginList::sortByCategory, isForwards);
                break;
            case manufacturerCol:
                list.sort(juce::KnownPluginList::sortByManufacturer, isForwards);
                break;
            case descCol:
                break;

            default:
                jassertfalse;
                break;
        }
    }

    juce::String MyTableListBoxModel::getPluginDescription(const juce::PluginDescription& desc)
    {
        juce::StringArray items;

        if (desc.descriptiveName != desc.name)
            items.add(desc.descriptiveName);

        items.add(desc.version);

        items.removeEmptyStrings();
        return items.joinIntoString(" - ");
    }
}