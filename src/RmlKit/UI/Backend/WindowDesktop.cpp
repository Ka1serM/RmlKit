#include "RmlKit/UI/Backend/Window.h"

#include <algorithm>
#include <format>

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_vulkan.h>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

//! Window coordinates are pixels on X11 and Windows, and points on Wayland and macOS.
SDL_Window* Window::create(const char* title, const int width, const int height) {
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const float scale = SDL_GetDisplayContentScale(display);
    SDL_Rect usable;
    if (!SDL_GetDisplayUsableBounds(display, &usable))
        fatal(std::format("SDL_GetDisplayUsableBounds failed: {}", SDL_GetError()));
    const int windowWidth = std::min(static_cast<int>(width * scale), usable.w);
    const int windowHeight = std::min(static_cast<int>(height * scale), usable.h);
    SDL_Window* window =
        SDL_CreateWindow(title, windowWidth, windowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window)
        fatal(std::format("SDL_CreateWindow failed: {}", SDL_GetError()));
    SDL_SetWindowMinimumSize(window, static_cast<int>(360 * scale), static_cast<int>(480 * scale));
    return window;
}

std::uintptr_t Window::getInstanceProcAddress() const {
    return reinterpret_cast<std::uintptr_t>(SDL_Vulkan_GetVkGetInstanceProcAddr());
}

std::vector<const char*> Window::getInstanceExtensions() const {
    Uint32 count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!extensions)
        fatal(std::format("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError()));
    return {extensions, extensions + count};
}

std::uintptr_t Window::createSurface(const std::uintptr_t instance) const {
    VkSurfaceKHR surface{};
    if (!SDL_Vulkan_CreateSurface(window.get(), reinterpret_cast<VkInstance>(instance), nullptr, &surface))
        fatal(std::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError()));
    return reinterpret_cast<std::uintptr_t>(surface);
}

NoorRHI::SurfaceSource Window::getSurfaceSource() const {
    using Kind = NoorRHI::SurfaceSource::Kind;
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window.get());
    if (void* display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr))
        return {.kind = Kind::Wayland,
                .display = reinterpret_cast<std::uintptr_t>(display),
                .window = reinterpret_cast<std::uintptr_t>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr))};
    if (void* handle = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr))
        return {.kind = Kind::Win32,
                .display = reinterpret_cast<std::uintptr_t>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr)),
                .window = reinterpret_cast<std::uintptr_t>(handle)};
    fatal("The window is not a Wayland or Win32 window.");
}

}
