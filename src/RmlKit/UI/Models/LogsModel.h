#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace RmlKit::UI {

struct LogLine {
    Rml::String time;
    Rml::String level;
    Rml::String source;
    Rml::String message;
    int index = -1;
};

struct LogRows {
    float above = 0;
    float below = 0;
    std::vector<LogLine> rows;
};

//! RmlUi lays out every row again on any layout change, so the list binds only the rows near its viewport.
struct LogsModel {
    static constexpr float rowHeight = 22;

    std::vector<LogLine> lines;
    int warningCount = 0;
    int errorCount = 0;
    int shownCount = 0;
    Rml::String search;
    Rml::String levelFilter;
    Rml::String sourceFilter;
    std::vector<Rml::String> sources;
    bool follow = true;
    int pickedRow = -1;
    int copiedRow = -1;
    LogLine picked;
    LogRows view;

    void add(LogLine line);
    void filter();
    bool setViewport(float top, float height, bool atEnd);

private:
    bool matches(const LogLine& line) const;
    void placeRows();
    std::pair<std::size_t, std::size_t> rowsNearViewport() const;

    std::vector<int> shown;
    float scrollTop = 0;
    float viewportHeight = 0;
    bool viewAtEnd = true;
    std::size_t rowsBegin = 0;
    std::size_t rowsEnd = 0;
};

void bind(Rml::DataModelConstructor& model, LogsModel& state);

}
