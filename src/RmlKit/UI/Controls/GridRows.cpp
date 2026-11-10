#include "RmlKit/UI/Controls/GridRows.h"

#include <algorithm>
#include <cmath>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

namespace {

template <typename Before>
std::size_t partitionPoint(std::size_t count, const Before& before) {
    std::size_t first = 0;
    while (count > 0) {
        const std::size_t half = count / 2;
        if (!before(first + half)) {
            count = half;
            continue;
        }
        first += half + 1;
        count -= half + 1;
    }
    return first;
}

}

GridRows::GridRows(const GridLayout layout) : layout(layout) {
    if (layout.minItemWidth <= 0 || layout.heightPerWidth < 0 || layout.headerHeight < 0 || layout.gap < 0)
        fatal("a grid layout has a positive item width and no negative size");
    if (layout.fixedHeight + layout.gap < layout.heightPerWidth * layout.gap)
        fatal("a grid layout has an item row whose height in dp is negative");
    headersBefore.push_back(0);
}

int GridRows::columnsFor(const float width) const {
    return std::max(1, static_cast<int>(std::floor((width + layout.gap) / (layout.minItemWidth + layout.gap))));
}

void GridRows::build(const int columnCount, const std::span<const GridSectionShape> sections) {
    if (columnCount <= 0)
        fatal("a grid has at least one column");
    columns = columnCount;
    const auto count = static_cast<float>(columns);
    itemRowWidthShare = layout.heightPerWidth / count;
    itemRowHeight = layout.fixedHeight + layout.gap - layout.heightPerWidth * layout.gap * (count - 1) / count;

    rowSections.clear();
    rowFirstItems.clear();
    headersBefore.assign(1, 0);
    uint32_t headers = 0;
    for (uint32_t section = 0; section < sections.size(); ++section) {
        if (sections[section].header) {
            rowSections.push_back(section);
            rowFirstItems.push_back(noItem);
            headersBefore.push_back(++headers);
        }
        for (uint32_t first = 0; first < sections[section].itemCount; first += static_cast<uint32_t>(columns)) {
            rowSections.push_back(section);
            rowFirstItems.push_back(first);
            headersBefore.push_back(headers);
        }
    }
}

std::pair<std::size_t, std::size_t> GridRows::between(const float from, const float to, const float width) const {
    const std::size_t firstBelow = partitionPoint(size(), [&](const std::size_t row) { return top(row, width) <= from; });
    const std::size_t firstAfter = partitionPoint(size(), [&](const std::size_t row) { return top(row, width) < to; });
    const std::size_t begin = firstBelow == 0 ? 0 : firstBelow - 1;
    return {begin, std::max(begin, firstAfter)};
}

GridExtent GridRows::extent(const std::size_t begin, const std::size_t end) const {
    const auto headers = static_cast<float>(headersBefore[end] - headersBefore[begin]);
    const auto itemRows = static_cast<float>(end - begin) - headers;
    return {.height = headers * layout.headerHeight + itemRows * itemRowHeight, .widthPercent = itemRows * itemRowWidthShare * 100};
}

float GridRows::top(const std::size_t row, const float width) const {
    const GridExtent above = extent(0, row);
    return above.height + above.widthPercent / 100 * width;
}

}
