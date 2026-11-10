#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace RmlKit::UI {

struct GridLayout {
    float minItemWidth = 0;
    float heightPerWidth = 0;
    float fixedHeight = 0;
    float headerHeight = 0;
    float gap = 0;
};

inline constexpr float wideThumbRatio = 104.0f / 283.0f;
inline constexpr GridLayout wideCardLayout{
    .minItemWidth = 238, .heightPerWidth = wideThumbRatio, .fixedHeight = 52 - 2 * wideThumbRatio, .headerHeight = 0, .gap = 12};

struct GridExtent {
    float height = 0;
    float widthPercent = 0;
};

struct GridSectionShape {
    bool header = false;
    uint32_t itemCount = 0;
};

class GridRows {
public:
    static constexpr uint32_t noItem = UINT32_MAX;

    explicit GridRows(GridLayout layout);

    const GridLayout& getLayout() const { return layout; }
    int getColumns() const { return columns; }
    std::size_t size() const { return rowSections.size(); }
    uint32_t getSection(std::size_t row) const { return rowSections[row]; }
    uint32_t getFirstItem(std::size_t row) const { return rowFirstItems[row]; }

    int columnsFor(float width) const;
    void build(int columnCount, std::span<const GridSectionShape> sections);
    std::pair<std::size_t, std::size_t> between(float from, float to, float width) const;
    GridExtent extent(std::size_t begin, std::size_t end) const;

private:
    float top(std::size_t row, float width) const;

    GridLayout layout;
    int columns = 0;
    float itemRowHeight = 0;
    float itemRowWidthShare = 0;
    std::vector<uint32_t> rowSections;
    std::vector<uint32_t> rowFirstItems;
    std::vector<uint32_t> headersBefore;
};

}
