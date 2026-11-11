#include "RmlKit/UI/Backend/Application.h"

#if PLATFORM_LINUX

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

#include "RmlKit/Core/Utf8Path.h"
#include "RmlKit/UI/Backend/EmbeddedResources.h"

namespace RmlKit::UI {

namespace {

constexpr std::string_view launcherResource = "RmlKit/Desktop/launcher.desktop";
constexpr std::string_view generatedMarker = "X-" PRODUCT_NAME "-Generated=true";

std::unexpected<Core::Error> failure(std::string message) {
    return Core::fail("desktop", std::move(message));
}

std::expected<std::filesystem::path, Core::Error> dataDirectory() {
    if (const char* base = std::getenv("XDG_DATA_HOME"); base && *base)
        return Core::pathFromUtf8(base);
    const char* home = std::getenv("HOME");
    if (!home || !*home)
        return failure("Neither XDG_DATA_HOME nor HOME is set, so the data folder is unknown.");
    return Core::pathFromUtf8(home) / ".local" / "share";
}

//! APPIMAGE names this app only when it runs from APPDIR: a child process inherits both from the AppImage that started it.
bool runsFromAppImage(const std::filesystem::path& executable) {
    const char* appDir = std::getenv("APPDIR");
    if (!appDir || !*appDir)
        return false;
    std::error_code error;
    const std::filesystem::path mount = std::filesystem::canonical(Core::pathFromUtf8(appDir), error);
    if (error)
        return false;
    const auto [end, inside] = std::ranges::mismatch(mount, executable);
    return end == mount.end();
}

std::expected<std::filesystem::path, Core::Error> executablePath() {
    std::error_code error;
    std::filesystem::path executable = std::filesystem::canonical("/proc/self/exe", error);
    if (error)
        return failure(std::format("Cannot find the executable: {}", error.message()));
    if (const char* appImage = std::getenv("APPIMAGE"); appImage && *appImage && runsFromAppImage(executable))
        return Core::pathFromUtf8(appImage);
    return executable;
}

//! The Exec key of the desktop entry spec quotes an argument and escapes these four characters in it.
std::string execArgument(const std::string& path) {
    std::string argument = "\"";
    for (const char c : path) {
        if (c == '\\' || c == '"' || c == '$' || c == '`')
            argument += '\\';
        argument += c;
    }
    return argument + '"';
}

std::expected<void, Core::Error> replace(std::string& text, const std::string_view placeholder, const std::string_view value) {
    const std::size_t position = text.find(placeholder);
    if (position == std::string::npos)
        return failure(std::format("The desktop entry has no {}.", placeholder));
    text.replace(position, placeholder.size(), value);
    return {};
}

std::expected<void, Core::Error> write(const std::filesystem::path& file, const std::string_view bytes) {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    if (error)
        return failure(std::format("Cannot create '{}': {}", Core::utf8(file.parent_path()), error.message()));
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream)
        return failure(std::format("Cannot write '{}'.", Core::utf8(file)));
    return {};
}

}

std::expected<void, Core::Error> Application::installDesktopEntry() {
    if (std::string_view(APP_ID).empty() || std::getenv("FLATPAK_ID") || !EmbeddedResources::exists(std::string(launcherResource)))
        return {};
    const bool svg = EmbeddedResources::exists("RmlKit/Desktop/icon.svg");
    const auto data = dataDirectory();
    if (!data)
        return std::unexpected(data.error());
    const std::filesystem::path launcher = *data / "applications" / APP_ID ".desktop";
    const std::filesystem::path icon =
        svg ? *data / "icons" / "hicolor" / "scalable" / "apps" / APP_ID ".svg" : *data / "icons" / "hicolor" / "256x256" / "apps" / APP_ID ".png";

    std::error_code error;
    if (std::filesystem::exists(launcher, error)) {
        std::ifstream stream(launcher, std::ios::binary);
        if (!stream)
            return failure(std::format("Cannot read '{}'.", Core::utf8(launcher)));
        const std::string existing(std::istreambuf_iterator<char>(stream), {});
        if (!existing.contains(generatedMarker))
            return {};
    }
    if (error)
        return failure(std::format("Cannot read '{}': {}", Core::utf8(launcher), error.message()));

    const auto executable = executablePath();
    if (!executable)
        return std::unexpected(executable.error());
    std::string entry(EmbeddedResources::read(std::string(launcherResource)));
    if (auto replaced = replace(entry, "{{EXECUTABLE}}", execArgument(Core::utf8(*executable))); !replaced)
        return replaced;
    if (auto replaced = replace(entry, "{{ICON}}", Core::utf8(icon)); !replaced)
        return replaced;
    if (auto written = write(icon, EmbeddedResources::read(svg ? "RmlKit/Desktop/icon.svg" : "RmlKit/Desktop/icon.png")); !written)
        return written;
    return write(launcher, entry);
}

}

#else

namespace RmlKit::UI {

std::expected<void, Core::Error> Application::installDesktopEntry() {
    return {};
}

}

#endif
