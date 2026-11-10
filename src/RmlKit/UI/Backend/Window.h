#pragma once

#include <memory>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Types.h>
#include <SDL3/SDL_video.h>

#include "RmlKit/UI/Backend/Renderer.h"

namespace RmlKit::UI {

class Window {
public:
    Window(const char* title, int width, int height);
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Window* getNative() const { return window.get(); }
    Renderer& getRenderer() { return renderer; }

    bool drawable() const;
    Rml::Vector2i pixelSize() const;
    void prepare(Rml::Context& context);
    void render(Rml::Context& context);

private:
    static SDL_Window* create(const char* title, int width, int height);
    static void setSwapInterval();

    struct WindowDeleter {
        void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
    };

    class GlContext {
    public:
        explicit GlContext(SDL_Window* window);
        ~GlContext();
        GlContext(const GlContext&) = delete;
        GlContext& operator=(const GlContext&) = delete;

    private:
        SDL_GLContext context;
    };

    std::unique_ptr<SDL_Window, WindowDeleter> window;
    GlContext gl;
    Renderer renderer;
    Rml::Vector2i viewportSize;
};

}
