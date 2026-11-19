#pragma once

#include <expected>
#include <memory>
#include <vector>
#include <optional>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include "RmlKit/Core/Error.h"
#include "RmlKit/UI/Backend/DevTools.h"
#include "RmlKit/UI/Backend/Dispatcher.h"
#include "RmlKit/UI/Backend/FileDrop.h"
#include "RmlKit/UI/Backend/LinkDrag.h"
#include "RmlKit/UI/Backend/LogRoute.h"
#include "RmlKit/UI/Backend/MessageBoxService.h"
#include "RmlKit/UI/Backend/RmlRuntime.h"
#include "RmlKit/UI/Backend/Router.h"
#include "RmlKit/UI/Backend/SecondaryWindow.h"
#include "RmlKit/UI/Backend/Window.h"
#include "RmlKit/UI/Backend/WindowView.h"
#include "RmlKit/UI/Controllers/LogsController.h"
#include "RmlKit/UI/Models/Model.h"

namespace RmlKit::UI {

class Application {
public:
    Application();
    virtual ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    virtual std::expected<void, Core::Error> startup() = 0;
    virtual std::expected<void, Core::Error> shutdown() = 0;

    void handle(const SDL_Event& event);
    bool iterate();
    void stopJobs() { dispatcher.stopJobs(); }

protected:
    Model& getModel() { return view.getModel(); }
    Router& getRouter() { return router; }
    Hotkeys& getHotkeys() { return view.getHotkeys(); }
    Rml::Context& getContext() { return view.getContext(); }
    Renderer& getRenderer() { return window.getRenderer(); }
    SDL_Window* getWindow() const { return window.getNative(); }
#if RMLKIT_VIDEO
    VideoStore& getVideos() { return rml.getVideos(); }
#endif

    void closeDocuments();
    //! The window stays valid until windowClosed returns for it.
    SecondaryWindow& openWindow(const WindowOptions& options);
    virtual void windowClosed(SecondaryWindow&) {}
    //! Sees every SDL event before RmlUi does; returning true keeps the event from RmlUi.
    virtual bool handleEvent(const SDL_Event&) { return false; }

private:
    class Sdl {
    public:
        Sdl();
        ~Sdl();
        Sdl(const Sdl&) = delete;
        Sdl& operator=(const Sdl&) = delete;

    private:
        static void setPlatformHints();
    };

    //! On Linux, writes the app's launcher and icon to the user's data folder, so the desktop lists the app and shows its icon on Wayland. It keeps a launcher that the user edited, and does nothing in a Flatpak, on Windows, on the web, or when rmlkit_add_app had no ICON.
    static std::expected<void, Core::Error> installDesktopEntry();
#if PLATFORM_WINDOWS
    static bool SDLCALL watchEvent(void* application, SDL_Event* event);
#endif
    static Uint64 SDLCALL wake(void* application, SDL_TimerID timer, Uint64 interval);

    double nextFrameDelay() const;
    void flushMotion();
    void dispatch(SDL_Event& event);
    void watchResize();
    void unwatchResize();
    void scheduleNextFrame();
    void scheduleWake(double seconds);
    void draw();
    void removeClosedWindows();
    //! Gives the event to the secondary window that it belongs to; false when it belongs to the main window.
    bool routeToWindow(SDL_Event& event);
#if PLATFORM_WEB
    bool routePointer(SDL_Event& event);
    SecondaryWindow* overlayAt(Rml::Vector2f point) const;
    void raise(SecondaryWindow& overlay);
#else
    SecondaryWindow* findWindow(const SDL_Event& event) const;
#endif

    Sdl sdl;
    Dispatcher dispatcher;
    NoorRHI::Device device;
    Window window;
    RmlRuntime rml;
    WindowView view;
    std::vector<std::unique_ptr<SecondaryWindow>> windows;
#if PLATFORM_WEB
    //! The overlay under the pointer, or the one that a held button keeps it in; null for the main window.
    SecondaryWindow* pointerWindow = nullptr;
    SecondaryWindow* keyWindow = nullptr;
    SecondaryWindow* draggedWindow = nullptr;
    Uint32 heldButtons = 0;
#endif
    LogRoute logRoute;
    LinkDrag linkDrag;
    FileDrop fileDrop;
    Router router;
    MessageBoxService messageBoxes;
    LogsController logs;
#ifndef NDEBUG
    std::optional<DevTools> devTools;
#endif
    bool running = true;
    bool framePending = true;
    std::optional<SDL_Event> pendingMotion;
    Uint32 wakeEvent = 0;
    SDL_TimerID wakeTimer = 0;
};

}
