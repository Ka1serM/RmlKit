#include "RmlKit/UI/Backend/Window.h"

#include <format>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

SDL_Window* Window::create(const char* title, const int width, const int height) {
    SDL_Window* window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_FILL_DOCUMENT);
    if (!window)
        fatal(std::format("SDL_CreateWindow failed: {}", SDL_GetError()));
    return window;
}

}
