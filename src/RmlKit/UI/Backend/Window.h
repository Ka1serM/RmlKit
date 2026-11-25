#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <NoorRHI/NoorRHI.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Types.h>
#include <SDL3/SDL_video.h>

#include "RmlKit/UI/Backend/Renderer.h"

namespace RmlKit::UI {

struct WindowOptions {
    Rml::String title = PRODUCT_NAME;
    int width = 1600;
    int height = 1056;
    int minimumWidth = 360;
    int minimumHeight = 480;
    SDL_Window* parent = nullptr;
    //! The Vulkan extensions of the device that the first window opens.
    const NoorRHI::ExtensionProvider* extensions = nullptr;
};

struct ContextRemover {
    void operator()(Rml::Context* context) const noexcept;
};

using ContextPointer = std::unique_ptr<Rml::Context, ContextRemover>;

class Window final : public NoorRHI::SurfaceProvider {
public:
    //! A context that the frame draws, with its top left corner at origin, in pixels; later layers draw on top.
    struct Layer {
        Rml::Context& context;
        Rml::Vector2i origin;
    };

    //! The first window opens the shared device, because Vulkan needs a surface to choose a queue that presents.
    Window(NoorRHI::Device& device, const WindowOptions& options);
    ~Window() override;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Window* getNative() const { return window.get(); }
    SDL_WindowID getId() const { return SDL_GetWindowID(window.get()); }
    Renderer& getRenderer() { return renderer; }

    ContextPointer openContext(const Rml::String& name, Rml::TextInputHandler& textInput);
    bool drawable() const;
    bool maximized() const;
    Rml::Vector2i pixelSize() const;
    void render(std::span<const Layer> layers);

#if !PLATFORM_WEB
    std::uintptr_t getInstanceProcAddress() const override;
    std::vector<const char*> getInstanceExtensions() const override;
    std::uintptr_t createSurface(std::uintptr_t instance) const override;
    NoorRHI::SurfaceSource getSurfaceSource() const override;
#endif
    std::uint32_t getWidth() const override { return static_cast<std::uint32_t>(pixelSize().x); }
    std::uint32_t getHeight() const override { return static_cast<std::uint32_t>(pixelSize().y); }

private:
    static SDL_Window* create(const WindowOptions& options);
    void openDevice(const NoorRHI::ExtensionProvider* extensions);
    NoorRHI::Swapchain openSwapchain(const NoorRHI::ExtensionProvider* extensions);
    //! NoorRHI falls back to an opaque swapchain when the driver cannot compose alpha.
    static constexpr bool transparent = !PLATFORM_WEB;
    NoorRHI::Frame acquire();
    void waitForNextFrame();

    struct WindowDeleter {
        void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
    };

    std::unique_ptr<SDL_Window, WindowDeleter> window;
    NoorRHI::Device& device;
    NoorRHI::Swapchain swapchain;
    Renderer renderer;
};

}
