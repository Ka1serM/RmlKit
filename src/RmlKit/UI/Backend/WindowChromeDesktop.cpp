#include "RmlKit/UI/Backend/WindowChrome.h"

#include <format>

#include <SDL3/SDL.h>

#if PLATFORM_WINDOWS
#include <dwmapi.h>
#endif

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

#if PLATFORM_WINDOWS
namespace {

void styleWindowFrame(SDL_Window* window) {
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    const auto handle = static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!handle)
        fatal("The SDL window has no Win32 handle.");
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(handle, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    const COLORREF frame = RGB(0x2f, 0x2f, 0x2f);
    DwmSetWindowAttribute(handle, DWMWA_BORDER_COLOR, &frame, sizeof(frame));
}

}
#endif

void WindowChrome::attach() {
    if (!SDL_SetWindowHitTest(window, hitTest, this))
        fatal(std::format("SDL_SetWindowHitTest failed: {}", SDL_GetError()));
#if PLATFORM_WINDOWS
    styleWindowFrame(window);
#endif
}

void WindowChrome::detach() {
    SDL_SetWindowHitTest(window, nullptr, nullptr);
}

}
