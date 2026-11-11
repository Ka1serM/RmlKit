#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <NoorRHI/NoorRHI.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Types.h>
#include <SDL3/SDL_video.h>

#include "RmlKit/UI/Backend/Renderer.h"

namespace RmlKit::UI {

class Window final : public NoorRHI::SurfaceProvider {
public:
    Window(const char* title, int width, int height);
    ~Window() override;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Window* getNative() const { return window.get(); }
    Renderer& getRenderer() { return renderer; }

    bool drawable() const;
    Rml::Vector2i pixelSize() const;
    void prepare(Rml::Context& context);
    void render(Rml::Context& context);

#if !PLATFORM_WEB
    std::uintptr_t getInstanceProcAddress() const override;
    std::vector<const char*> getInstanceExtensions() const override;
    std::uintptr_t createSurface(std::uintptr_t instance) const override;
    NoorRHI::SurfaceSource getSurfaceSource() const override;
#endif
    std::uint32_t getWidth() const override { return static_cast<std::uint32_t>(pixelSize().x); }
    std::uint32_t getHeight() const override { return static_cast<std::uint32_t>(pixelSize().y); }

private:
    static SDL_Window* create(const char* title, int width, int height);
    NoorRHI::Device openDevice();
    NoorRHI::Swapchain openSwapchain();
    NoorRHI::Frame acquire();

    struct WindowDeleter {
        void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
    };

    std::unique_ptr<SDL_Window, WindowDeleter> window;
    NoorRHI::Device device;
    NoorRHI::Swapchain swapchain;
    Renderer renderer;
};

}
