#include "RmlKit/UI/Backend/WindowChrome.h"

#include <string_view>
#include <utility>

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <SDL3/SDL.h>

#include "RmlKit/UI/Models/Model.h"

namespace RmlKit::UI {

namespace {

bool isEmptyLayoutBox(const Rml::Element& element) {
    const Rml::String& tag = element.GetTagName();
    if (tag != "div" && tag != "section" && tag != "header" && tag != "main" && tag != "footer")
        return false;

    const Rml::ComputedValues& style = element.GetComputedValues();
    if (style.background_color().alpha != 0 || style.has_decorator() || style.border_top_width() != 0 || style.border_right_width() != 0
        || style.border_bottom_width() != 0 || style.border_left_width() != 0)
        return false;
    for (const auto& [name, value] : element.GetAttributes())
        if (name.starts_with("data-event-"))
            return false;
    return true;
}

}

WindowChrome::WindowChrome(Rml::Context& context, Model& model, SDL_Window* window, std::move_only_function<void()> close)
    : context(context), window(window), close(std::move(close)), wayland(std::string_view(SDL_GetCurrentVideoDriver()) == "wayland") {
    model.bindEvent("closeWindow", &WindowChrome::closeWindow, this);
    model.bindEvent("minimizeWindow", &WindowChrome::minimizeWindow, this);
    model.bindEvent("toggleMaximized", &WindowChrome::toggleMaximized, this);
    attach();
}

WindowChrome::~WindowChrome() {
    detach();
}

void WindowChrome::closeWindow(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
    close();
}

void WindowChrome::minimizeWindow(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
    SDL_MinimizeWindow(window);
}

void WindowChrome::handle(const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        const SDL_MouseButtonEvent& button = event.button;
        if (button.button == SDL_BUTTON_LEFT && button.clicks == 2 && isDragRegion(button.x, button.y))
            toggleMaximized();
        return;
    }

    //! On Wayland the compositor takes a press on the drag region, so two mouse leaves within the double click time are a double click.
    if (!wayland)
        return;
    if (event.type == SDL_EVENT_WINDOW_MOUSE_ENTER && maximizeAfterMove) {
        maximizeAfterMove = false;
        toggleMaximized();
        return;
    }
    if (event.type != SDL_EVENT_WINDOW_MOUSE_LEAVE || lastHitTest != SDL_HITTEST_DRAGGABLE)
        return;
    constexpr Uint64 doubleClickMs = 500;
    const Uint64 now = SDL_GetTicks();
    if (now - lastDragPress > doubleClickMs) {
        lastDragPress = now;
        return;
    }
    lastDragPress = 0;
    maximizeAfterMove = true;
}

void WindowChrome::toggleMaximized(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
    toggleMaximized();
}

void WindowChrome::toggleMaximized() {
    //! Wayland sets SDL_WINDOW_MAXIMIZED asynchronously and ignores a second command during the change.
    const Uint64 now = SDL_GetTicks();
    if (now - lastMaximizeToggle < 250)
        return;
    lastMaximizeToggle = now;
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED)
        SDL_RestoreWindow(window);
    else
        SDL_MaximizeWindow(window);
}

SDL_HitTestResult SDLCALL WindowChrome::hitTest(SDL_Window*, const SDL_Point* area, void* chrome) {
    WindowChrome& self = *static_cast<WindowChrome*>(chrome);
    self.lastHitTest = self.hitTestAt(*area);
    return self.lastHitTest;
}

SDL_HitTestResult WindowChrome::hitTestAt(const SDL_Point area) const {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    if (flags & SDL_WINDOW_MAXIMIZED)
        return SDL_HITTEST_NORMAL;

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    constexpr int resizeBorder = 8;
    const bool resizable = flags & SDL_WINDOW_RESIZABLE;
    const bool left = resizable && area.x < resizeBorder;
    const bool right = resizable && area.x >= width - resizeBorder;
    const bool top = resizable && area.y < resizeBorder;
    const bool bottom = resizable && area.y >= height - resizeBorder;

    if (top && left)
        return SDL_HITTEST_RESIZE_TOPLEFT;
    if (top && right)
        return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (bottom && left)
        return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (bottom && right)
        return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (top)
        return SDL_HITTEST_RESIZE_TOP;
    if (right)
        return SDL_HITTEST_RESIZE_RIGHT;
    if (bottom)
        return SDL_HITTEST_RESIZE_BOTTOM;
    if (left)
        return SDL_HITTEST_RESIZE_LEFT;

    return isDragRegion(static_cast<float>(area.x), static_cast<float>(area.y)) ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

bool WindowChrome::isDragRegion(const float x, const float y) const {
    //! RmlUi's hit test skips transparent layout boxes, so empty space reaches the drag region.
    const float density = SDL_GetWindowPixelDensity(window);
    const Rml::Vector2f point{x * density, y * density};
    Rml::Element* ignored = nullptr;
    for (int depth = 0; depth < 16; ++depth) {
        Rml::Element* hit = context.GetElementAtPoint(point, ignored);
        if (!hit)
            return false;
        for (Rml::Element* element = hit; element; element = element->GetParentNode())
            if (element->IsClassSet("window-drag-region"))
                return true;
        if (!isEmptyLayoutBox(*hit))
            return false;
        ignored = hit;
    }
    return false;
}

}
