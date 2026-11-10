#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Types.h>

#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Controls/GridRows.h"
#include "RmlKit/UI/Models/GridRow.h"
#include "RmlKit/UI/Models/Model.h"

namespace RmlKit::UI {

template <typename Item>
struct GridSection {
    Rml::String icon;
    Rml::String title;
    Rml::String countText;
    std::vector<Item> items;
    bool collapsed = false;
};

template <typename Item>
class VirtualGrid {
public:
    VirtualGrid(Model& model, const Rml::String& viewportEvent, Rml::String dirtyVariable, GridModel<Item>& view, const GridLayout& layout)
        : model(model), dirtyVariable(std::move(dirtyVariable)), view(view), rows(layout) {
        model.bindEvent(viewportEvent, &VirtualGrid::onViewport, this);
    }
    VirtualGrid(const VirtualGrid&) = delete;
    VirtualGrid& operator=(const VirtualGrid&) = delete;

    void setSections(std::vector<GridSection<Item>> newSections) {
        sections = std::move(newSections);
        shapes.clear();
        shapes.reserve(sections.size());
        std::size_t itemCount = 0;
        for (const GridSection<Item>& section : sections) {
            const auto shown = section.collapsed ? 0 : static_cast<uint32_t>(section.items.size());
            shapes.push_back({.header = !section.title.empty(), .itemCount = shown});
            itemCount += section.items.size();
        }
        view.itemCount = static_cast<int>(itemCount);
        if (rows.getColumns() > 0)
            build(rows.getColumns());
        showRows(true);
    }

    template <typename IsCurrent>
    const Item* step(IsCurrent isCurrent, const int across, const int down) {
        if (rows.getColumns() <= 0 || (across == 0 && down == 0))
            return nullptr;
        std::vector<std::size_t> itemRows;
        std::optional<std::pair<std::ptrdiff_t, int>> current;
        for (std::size_t row = 0; row < rows.size(); ++row) {
            if (rows.getFirstItem(row) == GridRows::noItem)
                continue;
            if (!current)
                for (int column = 0; column < itemCount(row); ++column)
                    if (isCurrent(itemAt(row, column))) {
                        current.emplace(static_cast<std::ptrdiff_t>(itemRows.size()), column);
                        break;
                    }
            itemRows.push_back(row);
        }
        if (itemRows.empty())
            return nullptr;
        const auto rowCount = static_cast<std::ptrdiff_t>(itemRows.size());
        std::ptrdiff_t row = 0;
        int column = 0;
        if (current) {
            std::tie(row, column) = *current;
            if (down != 0) {
                row += down;
                if (row < 0 || row >= rowCount)
                    return nullptr;
                column = std::min(column, itemCount(itemRows[static_cast<std::size_t>(row)]) - 1);
            } else {
                column += across;
                while (column < 0) {
                    if (--row < 0)
                        return nullptr;
                    column += itemCount(itemRows[static_cast<std::size_t>(row)]);
                }
                while (column >= itemCount(itemRows[static_cast<std::size_t>(row)])) {
                    column -= itemCount(itemRows[static_cast<std::size_t>(row)]);
                    if (++row >= rowCount)
                        return nullptr;
                }
            }
        }
        const std::size_t reached = itemRows[static_cast<std::size_t>(row)];
        reveal(reached);
        return &itemAt(reached, column);
    }

private:
    int itemCount(const std::size_t row) const {
        const std::size_t count = sections[rows.getSection(row)].items.size() - rows.getFirstItem(row);
        return static_cast<int>(std::min<std::size_t>(count, static_cast<std::size_t>(rows.getColumns())));
    }

    const Item& itemAt(const std::size_t row, const int column) const {
        return sections[rows.getSection(row)].items[rows.getFirstItem(row) + static_cast<std::size_t>(column)];
    }

    float offsetOf(const GridExtent extent) const { return extent.height + extent.widthPercent / 100 * width; }

    void reveal(const std::size_t row) {
        const std::size_t first = row > 0 && rows.getFirstItem(row - 1) == GridRows::noItem ? row - 1 : row;
        const float top = offsetOf(rows.extent(0, first));
        const float bottom = offsetOf(rows.extent(0, row + 1));
        float target = scrollTop;
        if (top < scrollTop || bottom - top > viewportHeight)
            target = top;
        else if (bottom > scrollTop + viewportHeight)
            target = bottom - viewportHeight;
        if (target == scrollTop)
            return;
        scrollTop = target;
        view.scrollTo = std::format("{} {}", ++scrolls, target);
        showRows(false);
        model.dirty(dirtyVariable);
    }

    void onViewport(Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
        if (!arguments.empty())
            fatal("a viewport event takes no argument");
        scrollTop = event.GetParameter("scroll_top", 0.0f);
        viewportHeight = event.GetParameter("height", 0.0f);
        width = event.GetParameter("width", 0.0f);
        const int columns = rows.columnsFor(width);
        if (columns == rows.getColumns()) {
            showRows(false);
            return;
        }
        build(columns);
        showRows(true);
    }

    void build(const int columns) {
        rows.build(columns, shapes);
        //! RmlUi clamps the scroll top after the layout and sends no scroll event, so the grid clamps its copy the same way.
        const GridExtent content = rows.extent(0, rows.size());
        const float contentHeight = content.height + content.widthPercent / 100 * width;
        scrollTop = std::clamp(scrollTop, 0.0f, std::max(0.0f, contentHeight - viewportHeight));
    }

    void showRows(const bool force) {
        const float margin = viewportHeight / 2;
        const auto [begin, end] = rows.between(scrollTop - margin, scrollTop + viewportHeight + margin, width);
        if (!force && begin == shownBegin && end == shownEnd)
            return;
        shownBegin = begin;
        shownEnd = end;

        const GridLayout& layout = rows.getLayout();
        const int columns = rows.getColumns();
        view.rows.clear();
        view.rows.reserve(end - begin + 2);
        addSpacer(rows.extent(0, begin));
        for (std::size_t row = begin; row < end; ++row) {
            const GridSection<Item>& section = sections[rows.getSection(row)];
            GridRow<Item>& shown = view.rows.emplace_back();
            const uint32_t firstItem = rows.getFirstItem(row);
            if (firstItem == GridRows::noItem) {
                shown.header = true;
                shown.height = layout.headerHeight;
                shown.icon = section.icon;
                shown.title = section.title;
                shown.countText = section.countText;
                shown.collapsed = section.collapsed;
                continue;
            }
            const auto first = section.items.begin() + firstItem;
            const std::ptrdiff_t count = std::min<std::ptrdiff_t>(columns, section.items.end() - first);
            shown.items.assign(first, first + count);
            shown.emptyColumns = columns - static_cast<int>(count);
            shown.emptyGaps = layout.gap * static_cast<float>(std::max(0, shown.emptyColumns - 1));
        }
        addSpacer(rows.extent(end, rows.size()));
        model.dirty(dirtyVariable);
    }

    void addSpacer(const GridExtent extent) {
        GridRow<Item>& spacer = view.rows.emplace_back();
        spacer.height = extent.height;
        spacer.widthPercent = extent.widthPercent;
    }

    Model& model;
    Rml::String dirtyVariable;
    GridModel<Item>& view;
    GridRows rows;
    std::vector<GridSection<Item>> sections;
    std::vector<GridSectionShape> shapes;
    float scrollTop = 0;
    float viewportHeight = 0;
    float width = 0;
    std::size_t shownBegin = 0;
    std::size_t shownEnd = 0;
    std::uint64_t scrolls = 0;
};

}
