#include <cstdio>
#include <memory>

#if PLATFORM_WEB
#define SDL_MAIN_HANDLED
#include <emscripten/emscripten.h>
#else
#define SDL_MAIN_USE_CALLBACKS
#endif
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_messagebox.h>

#include RMLKIT_APPLICATION_HEADER

namespace {

SDL_AppResult fail(const RmlKit::Core::Error& error) {
    std::fprintf(stderr, "%s: %s\n", PRODUCT_NAME, error.message.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, PRODUCT_NAME, error.message.c_str(), nullptr);
    return SDL_APP_FAILURE;
}

}

#if PLATFORM_WEB

EM_ASYNC_JS(void, waitForAnimationFrame, (), { await new Promise(requestAnimationFrame); });

//! WebGPU waits suspend the stack through JSPI, which SDL's main callbacks on the web do not allow, so the app owns its loop.
int main() {
    auto app = std::make_unique<RMLKIT_APPLICATION_CLASS>();
    if (auto started = app->startup(); !started)
        return fail(started.error());
    bool running = true;
    while (running) {
        waitForAnimationFrame();
        SDL_Event event;
        while (SDL_PollEvent(&event))
            app->handle(event);
        running = app->iterate();
    }
    app->stopJobs();
    if (auto stopped = app->shutdown(); !stopped)
        return fail(stopped.error());
    return 0;
}

#else

SDL_AppResult SDL_AppInit(void** state, int, char**) {
    auto app = std::make_unique<RMLKIT_APPLICATION_CLASS>();
    if (auto started = app->startup(); !started)
        return fail(started.error());
    *state = app.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* state, SDL_Event* event) {
    static_cast<RMLKIT_APPLICATION_CLASS*>(state)->handle(*event);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* state) {
    return static_cast<RMLKIT_APPLICATION_CLASS*>(state)->iterate() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

void SDL_AppQuit(void* state, SDL_AppResult) {
    const std::unique_ptr<RMLKIT_APPLICATION_CLASS> app(static_cast<RMLKIT_APPLICATION_CLASS*>(state));
    if (!app)
        return;
    app->stopJobs();
    if (auto stopped = app->shutdown(); !stopped)
        fail(stopped.error());
}

#endif
