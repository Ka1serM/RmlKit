#include "RmlKit/UI/Backend/Application.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

#include <SDL3/SDL_hints.h>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

void Application::Sdl::setPlatformHints() {
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "waitevent");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    //! libdecor adds a client-side frame on Wayland, also to a borderless window, out of step with the app's frames during a resize.
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0");
}

//! On Wayland, SDL acks a configure only after its size event, so a draw in a watch commits the new size before the ack.
void Application::watchResize() {
#if PLATFORM_WINDOWS
    if (!SDL_AddEventWatch(watchEvent, this))
        fatal(std::format("SDL_AddEventWatch failed: {}", SDL_GetError()));
#endif
}

void Application::unwatchResize() {
#if PLATFORM_WINDOWS
    SDL_RemoveEventWatch(watchEvent, this);
#endif
}

void Application::scheduleNextFrame() {
    scheduleWake(nextFrameDelay());
}

void Application::scheduleWake(const double seconds) {
    if (wakeTimer)
        SDL_RemoveTimer(std::exchange(wakeTimer, 0));
    if (std::isinf(seconds))
        return;
    const auto nanoseconds = static_cast<Uint64>(std::ceil(std::max(seconds, 0.0) * SDL_NS_PER_SECOND));
    wakeTimer = SDL_AddTimerNS(nanoseconds, wake, this);
    if (!wakeTimer)
        fatal(std::format("SDL_AddTimerNS failed: {}", SDL_GetError()));
}

Uint64 SDLCALL Application::wake(void* application, SDL_TimerID, Uint64) {
    SDL_Event event{};
    event.type = static_cast<Application*>(application)->wakeEvent;
    SDL_PushEvent(&event);
    return 0;
}

#if PLATFORM_WINDOWS
bool SDLCALL Application::watchEvent(void* application, SDL_Event* event) {
    //! Windows runs a modal loop while the user drags the border, and SDL still sends live-resize exposes to watchers from inside it.
    const bool sizeChanged = event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
    const bool liveResizeExpose = event->type == SDL_EVENT_WINDOW_EXPOSED && event->window.data1 == 1;
    if (sizeChanged || liveResizeExpose)
        static_cast<Application*>(application)->draw();
    return true;
}
#endif

}
