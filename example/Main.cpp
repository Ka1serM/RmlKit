#include <cstdio>
#include <memory>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_messagebox.h>

#include "Application.h"

//! SDL's callbacks run the app the same way on the desktop and in the browser, where the page owns the main loop.
namespace {

SDL_AppResult fail(const RmlKit::Core::Error& error) {
    std::fprintf(stderr, "%s: %s\n", PRODUCT_NAME, error.message.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, PRODUCT_NAME, error.message.c_str(), nullptr);
    return SDL_APP_FAILURE;
}

}

SDL_AppResult SDL_AppInit(void** state, int, char**) {
    auto app = std::make_unique<Example::Application>();
    if (auto started = app->startup(); !started)
        return fail(started.error());
    *state = app.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* state, SDL_Event* event) {
    static_cast<Example::Application*>(state)->handle(*event);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* state) {
    return static_cast<Example::Application*>(state)->iterate() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

void SDL_AppQuit(void* state, SDL_AppResult) {
    const std::unique_ptr<Example::Application> app(static_cast<Example::Application*>(state));
    if (!app)
        return;
    app->stopJobs();
    if (auto stopped = app->shutdown(); !stopped)
        fail(stopped.error());
}
