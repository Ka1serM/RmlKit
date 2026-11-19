#include "RmlKit/UI/Backend/WindowButtonLayout.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

#if PLATFORM_LINUX
#include <dlfcn.h>
#endif

#include <SDL3/SDL_stdinc.h>

#include "RmlKit/UI/Backend/GlyphWindowButtons.h"
#include "RmlKit/UI/Backend/GtkWindowButtons.h"

namespace RmlKit::UI {

namespace {

#if PLATFORM_LINUX

std::filesystem::path configHome() {
    if (const char* config = std::getenv("XDG_CONFIG_HOME"); config && *config)
        return config;
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "") / ".config";
}

//! The value of a key in one section of an INI file such as kwinrc or the GTK settings.ini.
std::optional<std::string> readIni(const std::filesystem::path& path, const std::string_view section, const std::string_view key) {
    std::ifstream file(path);
    std::string line;
    bool inSection = false;
    while (std::getline(file, line)) {
        if (line.starts_with('[')) {
            inSection = line == "[" + std::string(section) + "]";
            continue;
        }
        const std::size_t equals = line.find('=');
        if (!inSection || equals == std::string::npos)
            continue;
        std::string_view name = std::string_view(line).substr(0, equals);
        while (!name.empty() && name.back() == ' ')
            name.remove_suffix(1);
        if (name != key)
            continue;
        std::string value = line.substr(equals + 1);
        value.erase(0, value.find_first_not_of(' '));
        return value;
    }
    return std::nullopt;
}

//! GSettings through a GIO loaded at run time, so the app does not link GLib; nothing is read when GIO or the schema is missing.
std::optional<std::string> readGSettings(const char* schemaId, const char* key) {
    void* gio = dlopen("libgio-2.0.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (!gio)
        return std::nullopt;
    using SourceDefault = void* (*)();
    using SourceLookup = void* (*)(void*, const char*, int);
    using SchemaHasKey = int (*)(void*, const char*);
    using SchemaUnref = void (*)(void*);
    using SettingsNew = void* (*)(const char*);
    using GetString = char* (*)(void*, const char*);
    using Unref = void (*)(void*);
    const auto sourceDefault = reinterpret_cast<SourceDefault>(dlsym(gio, "g_settings_schema_source_get_default"));
    const auto sourceLookup = reinterpret_cast<SourceLookup>(dlsym(gio, "g_settings_schema_source_lookup"));
    const auto schemaHasKey = reinterpret_cast<SchemaHasKey>(dlsym(gio, "g_settings_schema_has_key"));
    const auto schemaUnref = reinterpret_cast<SchemaUnref>(dlsym(gio, "g_settings_schema_unref"));
    const auto settingsNew = reinterpret_cast<SettingsNew>(dlsym(gio, "g_settings_new"));
    const auto getString = reinterpret_cast<GetString>(dlsym(gio, "g_settings_get_string"));
    const auto objectUnref = reinterpret_cast<Unref>(dlsym(gio, "g_object_unref"));
    const auto gFree = reinterpret_cast<Unref>(dlsym(gio, "g_free"));
    std::optional<std::string> value;
    void* source = sourceDefault && sourceLookup && schemaHasKey && schemaUnref && settingsNew && getString && objectUnref && gFree ? sourceDefault() : nullptr;
    if (void* schema = source ? sourceLookup(source, schemaId, 1) : nullptr) {
        if (schemaHasKey(schema, key)) {
            void* settings = settingsNew(schemaId);
            char* text = getString(settings, key);
            value = text ? std::optional<std::string>(text) : std::nullopt;
            gFree(text);
            objectUnref(settings);
        }
        schemaUnref(schema);
    }
    //! GIO registers types that cannot be unregistered, so the library stays loaded.
    return value;
}

std::vector<Rml::String> parseKWinSide(const std::string_view letters) {
    std::vector<Rml::String> buttons;
    for (const char letter : letters) {
        if (letter == 'I')
            buttons.emplace_back("minimize");
        else if (letter == 'A')
            buttons.emplace_back("maximize");
        else if (letter == 'X')
            buttons.emplace_back("close");
    }
    return buttons;
}

bool desktopIs(const std::string_view name) {
    const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
    return desktop && std::string_view(desktop).find(name) != std::string_view::npos;
}

WindowButtonLayout readDesktopLayout() {
    if (desktopIs("KDE")) {
        const std::filesystem::path kwinrc = configHome() / "kwinrc";
        const std::string left = readIni(kwinrc, "org.kde.kdecoration2", "ButtonsOnLeft").value_or("MS");
        const std::string right = readIni(kwinrc, "org.kde.kdecoration2", "ButtonsOnRight").value_or("HIAX");
        return {"kde", parseKWinSide(left), parseKWinSide(right)};
    }
    if (const std::optional<std::string> layout = readGSettings("org.gnome.desktop.wm.preferences", "button-layout"))
        return parseGnomeLayout(*layout);
    if (const std::optional<std::string> layout = readIni(configHome() / "gtk-3.0" / "settings.ini", "Settings", "gtk-decoration-layout"))
        return parseGnomeLayout(*layout);
    return {"gnome", {}, {"minimize", "maximize", "close"}};
}

//! The GNOME look comes from GTK itself, so the buttons follow the theme; KDE keeps the drawn buttons.
WindowButtonLayout readLinuxLayout() {
    WindowButtonLayout layout = readDesktopLayout();
    if (layout.style != "gnome")
        return layout;
    if (std::optional<GtkWindowButtons> buttons = renderGtkWindowButtons()) {
        layout.style = "gtk";
        layout.files = std::move(buttons->files);
    }
    return layout;
}

#endif

#if PLATFORM_WINDOWS

//! Windows has no call that draws caption buttons for a custom title bar; Windows Terminal and WinUI draw the glyphs of the system icon font, so this does too.
WindowButtonLayout readWindowsLayout() {
    WindowButtonLayout layout{"windows", {}, {"minimize", "maximize", "close"}};
    const char* windows = SDL_getenv("WINDIR");
    if (!windows || !*windows)
        return layout;
    for (const char* name : {"SegoeIcons.ttf", "segmdl2.ttf"})
        if (auto files = renderGlyphWindowButtons(std::filesystem::path(windows) / "Fonts" / name)) {
            layout.files = std::move(*files);
            break;
        }
    return layout;
}

#endif

}

const WindowButtonLayout& getWindowButtonLayout() {
#if PLATFORM_LINUX
    static const WindowButtonLayout layout = readLinuxLayout();
#elif defined(__APPLE__)
    static const WindowButtonLayout layout{"macos", {"close", "minimize", "maximize"}, {}};
#else
    static const WindowButtonLayout layout = readWindowsLayout();
#endif
    return layout;
}

}
