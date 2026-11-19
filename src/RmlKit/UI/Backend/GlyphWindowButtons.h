#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace RmlKit::UI {

//! Draws the caption glyphs of a Windows icon font, Segoe Fluent Icons or Segoe MDL2 Assets, as Windows draws them in a custom title bar: PNG files and a style.rcss for /UI/Platform/WindowButtons. Nothing if the font is missing or lacks a glyph.
std::optional<std::map<std::string, std::string, std::less<>>> renderGlyphWindowButtons(const std::filesystem::path& font);

}
