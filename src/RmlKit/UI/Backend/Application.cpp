#include "RmlKit/UI/Backend/Application.h"

#include <cstdint>
#include <format>
#include <limits>
#include <algorithm>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

Application::Sdl::Sdl() {
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition");
    setPlatformHints();
    //! On Wayland, the app id must match the name of the launcher.
    if (!SDL_SetAppMetadata(PRODUCT_NAME, PRODUCT_VERSION, APP_ID))
        fatal(std::format("SDL_SetAppMetadata failed: {}", SDL_GetError()));
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        fatal(std::format("SDL_Init failed: {}", SDL_GetError()));
}

Application::Sdl::~Sdl() {
    SDL_Quit();
}

Application::Application()
    : window(device, {}), rml(window), view(window, "main", [this] { running = false; }), logRoute(rml.getSystem(), getModel()),
      linkDrag(getContext(), getModel(), window.getNative()), fileDrop(getContext(), window.getNative()), router(getContext(), getModel()),
      messageBoxes(getContext(), getModel()), logs(getModel()) {
#ifndef NDEBUG
    devTools.emplace(getContext(), router);
#endif
    wakeEvent = SDL_RegisterEvents(1);
    if (wakeEvent == 0)
        fatal(std::format("SDL_RegisterEvents failed: {}", SDL_GetError()));
    watchResize();
    if (auto installed = installDesktopEntry(); !installed)
        getModel().report(ReportLevel::Warning, "desktop", std::format("The launcher is not installed: {}", installed.error().message));
}

Application::~Application() {
    if (wakeTimer)
        SDL_RemoveTimer(wakeTimer);
    unwatchResize();
}

void Application::removeClosedWindows() {
    std::erase_if(windows, [this](const std::unique_ptr<SecondaryWindow>& secondary) {
        if (secondary->isOpen())
            return false;
        windowClosed(*secondary);
#if PLATFORM_WEB
        for (SecondaryWindow** reference : {&pointerWindow, &keyWindow, &draggedWindow})
            if (*reference == secondary.get())
                *reference = nullptr;
#endif
        framePending = true;
        return true;
    });
}

double Application::nextFrameDelay() const {
    double delay = view.nextFrameDelay();
    for (const std::unique_ptr<SecondaryWindow>& secondary : windows)
        delay = std::min(delay, secondary->getView().nextFrameDelay());
    return delay;
}

void Application::closeDocuments() {
#ifndef NDEBUG
    //! The debugger owns documents of the context, and RmlUi crashes when they unload before it shuts down.
    devTools.reset();
#endif
    getContext().UnloadAllDocuments();
    //! RmlUi releases unloaded documents, and with them their elements, in the next update.
    getContext().Update();
}

void Application::handle(const SDL_Event& event) {
    framePending = true;
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        if (pendingMotion
            && (pendingMotion->motion.windowID != event.motion.windowID || pendingMotion->motion.which != event.motion.which
                || pendingMotion->motion.state != event.motion.state))
            flushMotion();
        if (pendingMotion) {
            SDL_Event merged = event;
            merged.motion.xrel += pendingMotion->motion.xrel;
            merged.motion.yrel += pendingMotion->motion.yrel;
            pendingMotion = merged;
        } else {
            pendingMotion = event;
        }
        return;
    }
    flushMotion();
    SDL_Event copy = event;
    dispatch(copy);
}

void Application::flushMotion() {
    if (!pendingMotion)
        return;
    SDL_Event motion = *pendingMotion;
    pendingMotion.reset();
    dispatch(motion);
}

bool Application::iterate() {
    flushMotion();
    if (!running)
        return false;
    removeClosedWindows();
    constexpr double dueWithinSeconds = 0.001;
    if (framePending || nextFrameDelay() <= dueWithinSeconds) {
        framePending = false;
        draw();
    }
    scheduleNextFrame();
    return running;
}

void Application::dispatch(SDL_Event& event) {
    if (event.type == wakeEvent)
        return;
    if (dispatcher.dispatch(event))
        return;
    if (routeToWindow(event))
        return;
    if (handleEvent(event))
        return;
    linkDrag.handle(event);
    fileDrop.handle(event);
#ifndef NDEBUG
    if (devTools && devTools->handle(event))
        return;
#endif
    if (event.type == SDL_EVENT_QUIT) {
        running = false;
        return;
    }
    view.handle(event);
}

}
