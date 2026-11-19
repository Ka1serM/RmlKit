#include "RmlKit/UI/Backend/GlyphWindowButtons.h"

#include <array>
#include <cmath>
#include <format>
#include <utility>

#include <plutovg.h>

namespace RmlKit::UI {

namespace {

//! Windows draws a caption button 46 by 32 pixels with a 10 pixel glyph at 100% scale.
constexpr int buttonWidth = 46;
constexpr int buttonHeight = 32;
constexpr float glyphSize = 10.0f;
constexpr int imageScale = 2;
constexpr float backdropOpacity = 0.36f;

struct Glyph {
    const char* name;
    plutovg_codepoint_t codepoint;
};

constexpr std::array glyphs{Glyph{"minimize", 0xE921}, Glyph{"maximize", 0xE922}, Glyph{"restore", 0xE923}, Glyph{"close", 0xE8BB}};

void writePng(void* closure, void* data, const int size) {
    static_cast<std::string*>(closure)->append(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

std::optional<std::string> draw(plutovg_font_face_t* face, const plutovg_codepoint_t codepoint, const float opacity) {
    const float size = glyphSize * imageScale;
    float advance = 0.0f;
    plutovg_rect_t extents{};
    plutovg_font_face_get_glyph_metrics(face, size, codepoint, &advance, nullptr, &extents);
    if (extents.w <= 0.0f || extents.h <= 0.0f)
        return std::nullopt;
    const int width = buttonWidth * imageScale;
    const int height = buttonHeight * imageScale;
    plutovg_surface_t* surface = plutovg_surface_create(width, height);
    plutovg_canvas_t* canvas = plutovg_canvas_create(surface);
    plutovg_canvas_set_rgba(canvas, 1.0f, 1.0f, 1.0f, opacity);
    plutovg_canvas_set_font(canvas, face, size);
    //! The glyph box starts on a whole pixel, as Windows snaps it.
    const float x = std::round((static_cast<float>(width) - extents.w) * 0.5f) - extents.x;
    const float y = std::round((static_cast<float>(height) - extents.h) * 0.5f) - extents.y;
    plutovg_canvas_add_glyph(canvas, codepoint, x, y);
    plutovg_canvas_fill(canvas);
    plutovg_canvas_destroy(canvas);
    std::string png;
    const bool written = plutovg_surface_write_to_png_stream(surface, writePng, &png);
    plutovg_surface_destroy(surface);
    return written ? std::optional(std::move(png)) : std::nullopt;
}

}

std::optional<std::map<std::string, std::string, std::less<>>> renderGlyphWindowButtons(const std::filesystem::path& font) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(font, error))
        return std::nullopt;
    plutovg_font_face_t* face = plutovg_font_face_load_from_file(font.string().c_str(), 0);
    if (!face)
        return std::nullopt;
    std::map<std::string, std::string, std::less<>> files;
    for (const Glyph& glyph : glyphs)
        for (const auto& [suffix, opacity] : {std::pair{"", 1.0f}, std::pair{"-backdrop", backdropOpacity}}) {
            std::optional<std::string> png = draw(face, glyph.codepoint, opacity);
            if (!png) {
                plutovg_font_face_destroy(face);
                return std::nullopt;
            }
            files.emplace(std::format("{}{}.png", glyph.name, suffix), std::move(*png));
        }
    plutovg_font_face_destroy(face);
    const auto image = [](const char* name, const char* suffix) { return std::format("decorator: image(/UI/Platform/WindowButtons/{}{}.png contain);", name, suffix); };
    std::string style = ".window-buttons.windows .title-button svg { display: none; }\n";
    for (const char* suffix : {"", "-backdrop"}) {
        const std::string group = *suffix ? ".window-buttons.windows.unfocused" : ".window-buttons.windows";
        for (const char* name : {"minimize", "maximize", "close"})
            style += std::format("{} .title-button.{} {{ {} }}\n", group, name, image(name, suffix));
        style += std::format("{}.maximized .title-button.maximize {{ {} }}\n", group, image("restore", suffix));
    }
    //! An inactive window shows a button at full strength under the mouse, and its close button turns red.
    for (const char* name : {"minimize", "maximize", "close"})
        style += std::format(".window-buttons.windows.unfocused .title-button.{}:hover {{ {} }}\n", name, image(name, ""));
    style += std::format(".window-buttons.windows.unfocused.maximized .title-button.maximize:hover {{ {} }}\n", image("restore", ""));
    files.emplace("style.rcss", std::move(style));
    return files;
}

}
