#include "RmlKit/UI/Backend/Application.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <SDL3/SDL_hints.h>

namespace RmlKit::UI {

namespace {

Rml::Vector2f pointerPosition(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
        return {event.motion.x, event.motion.y};
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return {event.button.x, event.button.y};
    default:
        return {event.wheel.mouse_x, event.wheel.mouse_y};
    }
}

void movePointer(SDL_Event& event, const Rml::Vector2f offset) {
    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
        event.motion.x += offset.x;
        event.motion.y += offset.y;
        return;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        event.button.x += offset.x;
        event.button.y += offset.y;
        return;
    default:
        event.wheel.mouse_x += offset.x;
        event.wheel.mouse_y += offset.y;
    }
}

}

void Application::Sdl::setPlatformHints() {
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
}

void Application::watchResize() {}

void Application::unwatchResize() {}

void Application::scheduleNextFrame() {}

SecondaryWindow& Application::openWindow(const WindowOptions& options) {
    framePending = true;
    SecondaryWindow& overlay = *windows.emplace_back(std::make_unique<SecondaryWindow>(window, options));
    keyWindow = &overlay;
    return overlay;
}

SecondaryWindow* Application::overlayAt(const Rml::Vector2f point) const {
    const Rml::Vector2i pixel{static_cast<int>(std::floor(point.x)), static_cast<int>(std::floor(point.y))};
    for (auto overlay = windows.rbegin(); overlay != windows.rend(); ++overlay)
        if ((*overlay)->isOpen() && (*overlay)->getArea().Contains(pixel))
            return overlay->get();
    return nullptr;
}

void Application::raise(SecondaryWindow& overlay) {
    const auto found = std::ranges::find_if(windows, [&](const std::unique_ptr<SecondaryWindow>& secondary) { return secondary.get() == &overlay; });
    std::rotate(found, found + 1, windows.end());
}

bool Application::routePointer(SDL_Event& event) {
    const float density = SDL_GetWindowPixelDensity(window.getNative());
    if (heldButtons == 0) {
        SecondaryWindow* hovered = overlayAt(pointerPosition(event) * density);
        if (hovered != pointerWindow) {
            Rml::Context& left = pointerWindow ? pointerWindow->getContext() : getContext();
            left.ProcessMouseLeave();
            pointerWindow = hovered;
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        heldButtons |= SDL_BUTTON_MASK(event.button.button);
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        heldButtons &= ~SDL_BUTTON_MASK(event.button.button);
    if (!pointerWindow) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            keyWindow = nullptr;
        return false;
    }

    SecondaryWindow& overlay = *pointerWindow;
    if (event.type == SDL_EVENT_MOUSE_MOTION && draggedWindow == &overlay) {
        overlay.move({static_cast<int>(std::lround(event.motion.xrel * density)), static_cast<int>(std::lround(event.motion.yrel * density))});
        framePending = true;
        return true;
    }
    movePointer(event, Rml::Vector2f(overlay.getArea().Position()) / -density);
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        keyWindow = &overlay;
        raise(overlay);
        if (event.button.button == SDL_BUTTON_LEFT && overlay.getView().isTitleBarAt(event.button.x, event.button.y))
            draggedWindow = &overlay;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && heldButtons == 0)
        draggedWindow = nullptr;
    overlay.getView().handle(event);
    return true;
}

bool Application::routeToWindow(SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return routePointer(event);
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING:
        if (!keyWindow)
            return false;
        keyWindow->getView().handle(event);
        return true;
    default:
        return false;
    }
}

void Application::draw() {
    if (!view.prepare())
        return;
    std::vector<Window::Layer> layers;
    layers.reserve(windows.size() + 1);
    layers.push_back({getContext(), {}});
    for (const std::unique_ptr<SecondaryWindow>& secondary : windows) {
        secondary->move({});
        if (secondary->getView().prepare())
            layers.push_back({secondary->getContext(), secondary->getArea().Position()});
    }
    window.render(layers);
}

}
