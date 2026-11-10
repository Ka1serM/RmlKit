#pragma once

#include <expected>

#include "RmlKit/Core/Error.h"

namespace RmlKit::UI {

//! On Linux, writes the app's launcher and icon to the user's data folder, so the desktop lists the app and shows its icon on Wayland. It keeps a launcher that the user edited, and does nothing in a Flatpak, on Windows, on the web, or when rmlkit_add_app had no ICON.
std::expected<void, Core::Error> installDesktopEntry();

}
