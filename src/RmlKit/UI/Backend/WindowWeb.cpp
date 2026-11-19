#include "RmlKit/UI/Backend/Window.h"

#include <format>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

SDL_Window* Window::create(const WindowOptions& options) {
    SDL_Window* window = SDL_CreateWindow(options.title.c_str(), options.width, options.height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_FILL_DOCUMENT);
    if (!window)
        fatal(std::format("SDL_CreateWindow failed: {}", SDL_GetError()));
    return window;
}

}
