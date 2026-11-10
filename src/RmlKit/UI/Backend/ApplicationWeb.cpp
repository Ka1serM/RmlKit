#include "RmlKit/UI/Backend/Application.h"

#include <SDL3/SDL_hints.h>

namespace RmlKit::UI {

void Application::Sdl::setPlatformHints() {
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
}

void Application::watchResize() {}

void Application::unwatchResize() {}

void Application::scheduleNextFrame() {}

}
