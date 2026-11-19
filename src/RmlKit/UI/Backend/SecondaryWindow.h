#pragma once

#include "RmlKit/UI/Backend/Window.h"
#include "RmlKit/UI/Backend/WindowView.h"

namespace RmlKit::UI {

//! A window next to the main one; it closes itself and the application removes it before the next frame.
class SecondaryWindow {
public:
#if PLATFORM_WEB
    //! The browser gives the app one canvas, so the window is an overlay that the main window draws on top of its own documents.
    SecondaryWindow(Window& main, const WindowOptions& options);
#else
    SecondaryWindow(NoorRHI::Device& device, const WindowOptions& options);
#endif
    SecondaryWindow(const SecondaryWindow&) = delete;
    SecondaryWindow& operator=(const SecondaryWindow&) = delete;

    SDL_Window* getNative() const { return window.getNative(); }
    Renderer& getRenderer() { return window.getRenderer(); }
    Rml::Context& getContext() { return view.getContext(); }
    Model& getModel() { return view.getModel(); }
    WindowView& getView() { return view; }
    bool isOpen() const { return open; }
    void close() { open = false; }

#if PLATFORM_WEB
    //! The area of the overlay in pixels of the main window.
    const Rml::Rectanglei& getArea() const { return area; }
    //! Moves the overlay by offset pixels and keeps it inside the main window.
    void move(Rml::Vector2i offset);
#endif

private:
#if PLATFORM_WEB
    Window& window;
    Rml::Rectanglei area;
#else
    Window window;
#endif
    WindowView view;
    bool open = true;
};

}
