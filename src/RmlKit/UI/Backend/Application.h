#pragma once

#include <expected>
#include <memory>
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
#include "RmlKit/UI/Backend/Tooltip.h"
#include "RmlKit/UI/Backend/Window.h"
#include "RmlKit/UI/Backend/WindowChrome.h"
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
    Model& getModel() { return model; }
    Router& getRouter() { return router; }
    Renderer& getRenderer() { return window.getRenderer(); }
    SDL_Window* getWindow() const { return window.getNative(); }
#if RMLKIT_VIDEO
    VideoStore& getVideos() { return rml.getVideos(); }
#endif

    void closeDocuments();

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

    struct ContextRemover {
        void operator()(Rml::Context* context) const noexcept { Rml::RemoveContext(context->GetName()); }
    };

    static std::unique_ptr<Rml::Context, ContextRemover> createContext(Rml::Vector2i size);
    static bool SDLCALL watchEvent(void* application, SDL_Event* event);
    static Uint64 SDLCALL wake(void* application, SDL_TimerID timer, Uint64 interval);

    double nextFrameDelay() const;
    void flushMotion();
    void dispatch(SDL_Event& event);
    void watchResize();
    void unwatchResize();
    void scheduleNextFrame();
    void scheduleWake(double seconds);
    void draw();
    void update();
    void refreshArrivedTextures();

    Sdl sdl;
    Dispatcher dispatcher;
    Window window;
    RmlRuntime rml;
    std::unique_ptr<Rml::Context, ContextRemover> context;
    Model model;
    LogRoute logRoute;
    WindowChrome chrome;
    LinkDrag linkDrag;
    FileDrop fileDrop;
    Router router;
    Tooltip tooltip;
    MessageBoxService messageBoxes;
#ifndef NDEBUG
    DevTools devTools;
#endif
    bool running = true;
    bool framePending = true;
    Uint64 lastUpdateNs = 0;
    std::optional<SDL_Event> pendingMotion;
    Uint32 wakeEvent = 0;
    SDL_TimerID wakeTimer = 0;
};

}
