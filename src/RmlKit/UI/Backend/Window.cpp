#include "RmlKit/UI/Backend/Window.h"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <utility>

#include "RmlKit/Core/Utf8Path.h"
#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {
namespace {

std::filesystem::path userCacheDirectory() {
#if PLATFORM_WINDOWS
    if (const char* local = std::getenv("LOCALAPPDATA"); local && *local)
        return Core::pathFromUtf8(local);
#elif PLATFORM_LINUX
    if (const char* cache = std::getenv("XDG_CACHE_HOME"); cache && *cache)
        return Core::pathFromUtf8(cache);
    if (const char* home = std::getenv("HOME"); home && *home)
        return Core::pathFromUtf8(home) / ".cache";
#endif
    return {};
}

std::filesystem::path pipelineDirectory() {
    const std::filesystem::path cache = userCacheDirectory();
    if (cache.empty())
        return {};
    return cache / PRODUCT_NAME / "pipelines";
}

}

Window::Window(const char* title, const int width, const int height)
    : window(create(title, width, height)), device(openDevice()), swapchain(openSwapchain()), renderer(device, swapchain.getFormat()) {}

Window::~Window() {
    if (auto idle = device.waitIdle(); !idle)
        fatal(std::format("The GPU did not finish its work: {}", idle.error().message));
}

NoorRHI::Device Window::openDevice() {
    NoorRHI::Device opened;
    if (auto result = opened.open({.applicationName = PRODUCT_NAME, .presentation = this, .pipelineDirectory = pipelineDirectory()}); !result)
        fatal(std::format("The GPU device cannot open: {}", result.error().message));
    return opened;
}

NoorRHI::Swapchain Window::openSwapchain() {
    NoorRHI::Swapchain opened;
    if (auto result = opened.open(device, *this, {.presentMode = NoorRHI::PresentMode::Vsync, .maxFramesInFlight = 2}); !result)
        fatal(std::format("The swapchain cannot open: {}", result.error().message));
    return opened;
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
    context.SetDimensions(pixelSize());
}

NoorRHI::Frame Window::acquire() {
    constexpr int attempts = 4;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        auto frame = swapchain.acquire();
        if (!frame)
            fatal(std::format("The swapchain cannot give a frame: {}", frame.error().message));
        if (*frame)
            return std::move(*frame);
    }
    fatal("The swapchain gives no frame after a rebuild.");
}

void Window::render(Rml::Context& context) {
    NoorRHI::Frame frame = acquire();
    const NoorRHI::Image& image = frame.getImage();
    renderer.beginFrame({static_cast<int>(image.getWidth()), static_cast<int>(image.getHeight())});
    context.Render();
    if (auto submitted = device.submit(renderer.endFrame(image)); !submitted)
        fatal(std::format("The GPU cannot run the frame: {}", submitted.error().message));
    if (auto presented = swapchain.present(std::move(frame)); !presented)
        fatal(std::format("The swapchain cannot present the frame: {}", presented.error().message));
    waitForNextFrame();
}

//! On the web, a wait that yields between the resize of the canvas and its draw shows the cleared canvas.
void Window::waitForNextFrame() {
    if (auto ready = swapchain.wait(); !ready)
        fatal(std::format("The swapchain cannot wait for a frame: {}", ready.error().message));
}

}
