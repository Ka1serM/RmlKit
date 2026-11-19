#include "RmlKit/UI/Backend/WindowButtonLayout.h"

#include <string_view>

namespace RmlKit::UI {

namespace {

std::vector<Rml::String> parseSide(std::string_view side) {
    std::vector<Rml::String> buttons;
    while (!side.empty()) {
        const std::size_t comma = side.find(',');
        std::string_view name = side.substr(0, comma);
        while (!name.empty() && name.front() == ' ')
            name.remove_prefix(1);
        while (!name.empty() && name.back() == ' ')
            name.remove_suffix(1);
        if (name == "minimize" || name == "maximize" || name == "close")
            buttons.emplace_back(name);
        if (comma == std::string_view::npos)
            break;
        side.remove_prefix(comma + 1);
    }
    return buttons;
}

}

WindowButtonLayout parseGnomeLayout(const std::string_view layout) {
    const std::size_t colon = layout.find(':');
    if (colon == std::string_view::npos)
        return {"gnome", parseSide(layout), {}};
    return {"gnome", parseSide(layout.substr(0, colon)), parseSide(layout.substr(colon + 1))};
}

std::optional<std::string_view> readPlatformFile(std::string_view path) {
    constexpr std::string_view folder = "UI/Platform/WindowButtons/";
    if (path.starts_with('/'))
        path.remove_prefix(1);
    if (!path.starts_with(folder))
        return std::nullopt;
    path.remove_prefix(folder.size());
    const WindowButtonLayout& layout = getWindowButtonLayout();
    if (const auto file = layout.files.find(path); file != layout.files.end())
        return std::string_view(file->second);
    //! RmlUi rejects a style sheet without a rule.
    if (path == "style.rcss")
        return std::string_view(".window-buttons {}");
    return std::nullopt;
}

}
