#include "RmlKit/UI/Backend/Window.h"

#include <format>

#include <RmlUi/Core/Log.h>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

Window::GlContext::GlContext(SDL_Window* window) : context(SDL_GL_CreateContext(window)) {
    if (!context)
        fatal(std::format("SDL_GL_CreateContext failed: {}", SDL_GetError()));
    if (!SDL_GL_MakeCurrent(window, context)) {
        SDL_GL_DestroyContext(context);
        fatal(std::format("SDL_GL_MakeCurrent failed: {}", SDL_GetError()));
    }
    setSwapInterval();
    Rml::String message;
    if (!RmlGL3::Initialize(&message)) {
        SDL_GL_DestroyContext(context);
        fatal(std::format("OpenGL could not be loaded: {}", message));
    }
    Rml::Log::Message(Rml::Log::LT_INFO, "%s", message.c_str());
}

Window::GlContext::~GlContext() {
    RmlGL3::Shutdown();
    SDL_GL_DestroyContext(context);
}

Window::Window(const char* title, const int width, const int height) : window(create(title, width, height)), gl(window.get()), viewportSize(pixelSize()) {
    renderer.SetViewport(viewportSize.x, viewportSize.y);
}

bool Window::drawable() const {
    constexpr SDL_WindowFlags hidden = SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN | SDL_WINDOW_OCCLUDED;
    if (SDL_GetWindowFlags(window.get()) & hidden)
        return false;
    const Rml::Vector2i size = pixelSize();
    return size.x > 0 && size.y > 0;
}

Rml::Vector2i Window::pixelSize() const {
    Rml::Vector2i size;
    if (!SDL_GetWindowSizeInPixels(window.get(), &size.x, &size.y))
        fatal(std::format("SDL_GetWindowSizeInPixels failed: {}", SDL_GetError()));
    return size;
}

void Window::prepare(Rml::Context& context) {
    const Rml::Vector2i size = pixelSize();
    if (size != viewportSize) {
        renderer.SetViewport(size.x, size.y);
        viewportSize = size;
    }
    context.SetDimensions(size);
}

void Window::render(Rml::Context& context) {
    renderer.Clear();
    renderer.BeginFrame();
    context.Render();
    renderer.EndFrame();
    if (!SDL_GL_SwapWindow(window.get()))
        fatal(std::format("SDL_GL_SwapWindow failed: {}", SDL_GetError()));
}

}
