#include "RmlKit/UI/Models/LogsModel.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

#include "RmlKit/Core/StdGaps.h"

namespace RmlKit::UI {

namespace {

constexpr std::size_t overscanRows = 2;

char lowerAscii(const char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool containsIgnoringCase(const Rml::String& text, const Rml::String& part) {
    return !std::ranges::search(text, part, {}, lowerAscii, lowerAscii).empty();
}

}

void LogsModel::add(LogLine line) {
    if (line.level == "WARN")
        ++warningCount;
    if (line.level == "ERR")
        ++errorCount;
    if (!std::ranges::contains(sources, line.source))
        sources.push_back(line.source);
    line.index = static_cast<int>(lines.size());
    if (matches(line))
        shown.push_back(line.index);
    shownCount = static_cast<int>(shown.size());
    lines.push_back(std::move(line));
    placeRows();
}

void LogsModel::filter() {
    shown.clear();
    for (const LogLine& line : lines)
        if (matches(line))
            shown.push_back(line.index);
    shownCount = static_cast<int>(shown.size());
    placeRows();
}

bool LogsModel::setViewport(const float top, const float height, const bool atEnd) {
    scrollTop = top;
    viewportHeight = height;
    viewAtEnd = atEnd;
    if (rowsNearViewport() == std::pair(rowsBegin, rowsEnd))
        return false;
    placeRows();
    return true;
}

bool LogsModel::matches(const LogLine& line) const {
    if (!levelFilter.empty() && line.level != levelFilter)
        return false;
    if (!sourceFilter.empty() && line.source != sourceFilter)
        return false;
    if (search.empty())
        return true;
    return containsIgnoringCase(line.source, search) || containsIgnoringCase(line.message, search);
}

void LogsModel::placeRows() {
    std::tie(rowsBegin, rowsEnd) = rowsNearViewport();
    view.above = static_cast<float>(rowsBegin) * rowHeight;
    view.below = static_cast<float>(shown.size() - rowsEnd) * rowHeight;
    view.rows.clear();
    for (std::size_t row = rowsBegin; row < rowsEnd; ++row)
        view.rows.push_back(lines[static_cast<std::size_t>(shown[row])]);
}

std::pair<std::size_t, std::size_t> LogsModel::rowsNearViewport() const {
    const std::size_t count = shown.size();
    const float maxTop = std::max(0.0f, static_cast<float>(count) * rowHeight - viewportHeight);
    const float top = follow && viewAtEnd ? maxTop : std::clamp(scrollTop, 0.0f, maxTop);
    const auto first = static_cast<std::size_t>(top / rowHeight);
    const auto last = static_cast<std::size_t>(std::ceil((top + viewportHeight) / rowHeight));
    const std::size_t begin = std::min(count, first > overscanRows ? first - overscanRows : 0);
    return {begin, std::clamp(last + overscanRows, begin, count)};
}

void bind(Rml::DataModelConstructor& model, LogsModel& state) {
    auto screen = model.RegisterStruct<LogsModel>();
    screen.RegisterMember("warningCount", &LogsModel::warningCount);
    screen.RegisterMember("errorCount", &LogsModel::errorCount);
    screen.RegisterMember("shownCount", &LogsModel::shownCount);
    screen.RegisterMember("search", &LogsModel::search);
    screen.RegisterMember("levelFilter", &LogsModel::levelFilter);
    screen.RegisterMember("sourceFilter", &LogsModel::sourceFilter);
    screen.RegisterMember("sources", &LogsModel::sources);
    screen.RegisterMember("follow", &LogsModel::follow);
    screen.RegisterMember("pickedRow", &LogsModel::pickedRow);
    screen.RegisterMember("copiedRow", &LogsModel::copiedRow);
    screen.RegisterMember("picked", &LogsModel::picked);
    auto rows = model.RegisterStruct<LogRows>();
    rows.RegisterMember("above", &LogRows::above);
    rows.RegisterMember("below", &LogRows::below);
    rows.RegisterMember("rows", &LogRows::rows);
    model.Bind("logs", &state);
    model.Bind("logRows", &state.view);
    model.Bind("logLines", &state.lines);
}

}
