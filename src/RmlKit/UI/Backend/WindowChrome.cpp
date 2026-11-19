#include "RmlKit/UI/Backend/WindowChrome.h"

#include <string_view>
#include <utility>

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <SDL3/SDL.h>

#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Models/Model.h"

namespace RmlKit::UI {

namespace {

bool drawsBackground(const Rml::Element& element) {
    const Rml::ComputedValues& style = element.GetComputedValues();
    return style.background_color().alpha != 0 || style.has_decorator() || style.border_top_width() != 0 || style.border_right_width() != 0
           || style.border_bottom_width() != 0 || style.border_left_width() != 0;
}

bool hasEvent(const Rml::Element& element) {
    for (const auto& [name, value] : element.GetAttributes())
        if (name.starts_with("data-event-"))
            return true;
    return false;
}

bool isTransparent(const Rml::Element& element) {
    return !drawsBackground(element) && !hasEvent(element);
}

bool isControl(const Rml::Element& element) {
    const Rml::String& tag = element.GetTagName();
    return tag == "button" || tag == "input" || tag == "select" || tag == "textarea" || hasEvent(element);
}

bool isEmptyLayoutBox(const Rml::Element& element) {
    const Rml::String& tag = element.GetTagName();
    const bool layout = tag == "div" || tag == "section" || tag == "header" || tag == "main" || tag == "footer" || tag == "nav" || tag == "aside";
    return layout && isTransparent(element);
}

}

WindowChrome::WindowChrome(Rml::Context& context, Model& model, SDL_Window* window, std::move_only_function<void()> close)
    : context(context), model(model), window(window), close(std::move(close)), wayland(std::string_view(SDL_GetCurrentVideoDriver()) == "wayland") {
    model.bindWindowEvent("closeWindow", &WindowChrome::closeWindow, this);
    model.bindWindowEvent("minimizeWindow", &WindowChrome::minimizeWindow, this);
    model.bindWindowEvent("toggleMaximized", &WindowChrome::toggleMaximized, this);
    model.bindWindowEvent("windowButton", &WindowChrome::pressWindowButton, this);
    attach();
    showMaximized();
}

WindowChrome::~WindowChrome() {
    detach();
}

void WindowChrome::closeWindow(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
    close();
}

void WindowChrome::pressWindowButton(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& arguments) {
    const Rml::String button = arguments.size() == 1 ? arguments[0].Get<Rml::String>() : Rml::String();
    if (button == "close")
        closeWindow(handle, event, {});
    else if (button == "minimize")
        minimizeWindow(handle, event, {});
    else if (button == "maximize")
        toggleMaximized(handle, event, {});
    else
        fatal("windowButton takes minimize, maximize or close");
}

void WindowChrome::minimizeWindow(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
    SDL_MinimizeWindow(window);
}

void WindowChrome::showMaximized() {
    const bool maximized = SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN);
    if (model.windowMaximized == maximized)
        return;
    model.windowMaximized = maximized;
    model.dirty("windowMaximized");
}

void WindowChrome::handle(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        return showMaximized();
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        model.windowFocused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
        model.dirty("windowFocused");
        return;
    default:
        break;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        const SDL_MouseButtonEvent& button = event.button;
        if (button.button == SDL_BUTTON_LEFT && button.clicks == 2 && regionAt(button.x, button.y) == Region::TitleBar)
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
    if (event.type != SDL_EVENT_WINDOW_MOUSE_LEAVE || lastHitTest != SDL_HITTEST_DRAGGABLE || !lastHitTitleBar)
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
    self.lastHitTitleBar = self.lastHitTest == SDL_HITTEST_DRAGGABLE && self.regionAt(static_cast<float>(area->x), static_cast<float>(area->y)) == Region::TitleBar;
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

    return regionAt(static_cast<float>(area.x), static_cast<float>(area.y)) != Region::None ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

//! The hit test looks through transparent layout boxes and overlay documents, so a press on the window background drags the window; only a .window-drag-region is the title bar, where a double click maximizes.
WindowChrome::Region WindowChrome::regionAt(const float x, const float y) const {
    const float density = SDL_GetWindowPixelDensity(window);
    const Rml::Vector2f point{x * density, y * density};
    Rml::Element* ignored = nullptr;
    for (int depth = 0; depth < 16; ++depth) {
        Rml::Element* hit = context.GetElementAtPoint(point, ignored);
        if (!hit)
            return Region::None;
        for (Rml::Element* element = hit; element; element = element->GetParentNode()) {
            if (isControl(*element))
                return Region::None;
            if (element->IsClassSet("window-drag-region"))
                return Region::TitleBar;
        }
        const bool document = hit == static_cast<Rml::Element*>(hit->GetOwnerDocument());
        if (document && drawsBackground(*hit))
            return Region::Background;
        if (!document && !isEmptyLayoutBox(*hit))
            return Region::None;
        ignored = hit;
    }
    return Region::None;
}

}
