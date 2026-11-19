#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <RmlUi/Core/Types.h>

namespace RmlKit::UI {

//! The title bar buttons as the desktop places them: style is windows, macos, gnome or kde, and start and end hold minimize, maximize and close in order.
struct WindowButtonLayout {
    Rml::String style;
    std::vector<Rml::String> start;
    std::vector<Rml::String> end;
    //! Files under UI/Platform/WindowButtons/ by name, such as the button images that GTK drew for the style gtk.
    std::map<std::string, std::string, std::less<>> files;
};

//! Reads the layout once from the platform and the desktop settings: GNOME button-layout, the KWin buttons, or the GTK decoration layout.
const WindowButtonLayout& getWindowButtonLayout();

//! A file under UI/Platform/WindowButtons/ from the layout; style.rcss is there on every platform, without styles when the buttons are drawn by RmlKit.
std::optional<std::string_view> readPlatformFile(std::string_view path);

//! Parses a GNOME or GTK layout such as "close:minimize,maximize"; unknown names such as appmenu are left out.
WindowButtonLayout parseGnomeLayout(std::string_view layout);

}
