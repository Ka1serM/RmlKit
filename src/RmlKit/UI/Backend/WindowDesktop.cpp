#include "RmlKit/UI/Backend/Window.h"

#include <algorithm>
#include <format>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

//! Window coordinates are pixels on X11 and Windows, and points on Wayland and macOS.
SDL_Window* Window::create(const char* title, const int width, const int height) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const float scale = SDL_GetDisplayContentScale(display);
    SDL_Rect usable;
    if (!SDL_GetDisplayUsableBounds(display, &usable))
        fatal(std::format("SDL_GetDisplayUsableBounds failed: {}", SDL_GetError()));
    const int windowWidth = std::min(static_cast<int>(width * scale), usable.w);
    const int windowHeight = std::min(static_cast<int>(height * scale), usable.h);
    SDL_Window* window =
        SDL_CreateWindow(title, windowWidth, windowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window)
        fatal(std::format("SDL_CreateWindow failed: {}", SDL_GetError()));
    SDL_SetWindowMinimumSize(window, static_cast<int>(360 * scale), static_cast<int>(480 * scale));
    return window;
}

void Window::setSwapInterval() {
    if (!SDL_GL_SetSwapInterval(-1))
        SDL_GL_SetSwapInterval(1);
}

}
