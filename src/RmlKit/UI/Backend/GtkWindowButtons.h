#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>

namespace RmlKit::UI {

//! The window buttons as GTK draws them for the theme of the desktop: PNG images by name, such as close-hover.png, and a stylesheet that sizes the buttons and puts the images on them.
struct GtkWindowButtons {
    std::map<std::string, std::string, std::less<>> files;
};

//! Loads GTK 4 at run time, with libadwaita when it is there, and draws every button in every state once; nothing when GTK or a display is missing.
std::optional<GtkWindowButtons> renderGtkWindowButtons();

}
