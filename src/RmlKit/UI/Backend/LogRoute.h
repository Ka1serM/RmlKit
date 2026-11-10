#pragma once

#include <cstdarg>
#include <memory>

#include <RmlUi_Platform_SDL.h>

namespace RmlKit::UI {

class Model;

class LogSystemInterface final : public SystemInterface_SDL {
public:
    using SystemInterface_SDL::SystemInterface_SDL;

    void setModel(Model* model) { this->model = model; }
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
    void SetMouseCursor(const Rml::String& cursorName) override;
#if PLATFORM_WEB
    //! SDL has no browser clipboard: it keeps the text to itself. This also hands it to the page (ClipboardWeb.cpp).
    void SetClipboardText(const Rml::String& text) override;
#endif

private:
    Model* model = nullptr;
    std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> horizontalResize{SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE), &SDL_DestroyCursor};
    std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> verticalResize{SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE), &SDL_DestroyCursor};
};

class LogRoute {
public:
    LogRoute(LogSystemInterface& system, Model& model);
    ~LogRoute();
    LogRoute(const LogRoute&) = delete;
    LogRoute& operator=(const LogRoute&) = delete;

private:
#if RMLKIT_VIDEO
    static void logFfmpeg(void* context, int level, const char* format, va_list arguments);
#endif

    LogSystemInterface& system;
};

}
