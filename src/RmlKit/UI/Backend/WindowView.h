#pragma once

#include <functional>
#include <optional>

#include <RmlUi/Core/Context.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL_events.h>

#include "RmlKit/UI/Backend/Hotkeys.h"
#include "RmlKit/UI/Backend/TextMenu.h"
#include "RmlKit/UI/Backend/Tooltip.h"
#include "RmlKit/UI/Backend/Window.h"
#include "RmlKit/UI/Backend/WindowChrome.h"
#include "RmlKit/UI/Models/Model.h"

namespace RmlKit::UI {

class WindowView {
public:
    WindowView(Window& window, const Rml::String& name, std::move_only_function<void()> close);
    WindowView(const WindowView&) = delete;
    WindowView& operator=(const WindowView&) = delete;

    Window& getWindow() { return window; }
    Rml::Context& getContext() { return *context; }
    Model& getModel() { return model; }
    Hotkeys& getHotkeys() { return hotkeys; }

    //! An overlay view has its own size in pixels; otherwise the view fills its window.
    void setOverlaySize(Rml::Vector2i size) { overlaySize = size; }
    bool isTitleBarAt(float x, float y) const { return chrome.isTitleBarAt(x, y); }

    double nextFrameDelay() const;
    void handle(SDL_Event& event);
    //! Lays out the next frame; false when the window cannot draw now.
    bool prepare();
    void draw();

private:
    void update();
    void styleDocuments();
    void refreshArrivedTextures();

    Window& window;
    TextInputMethodEditor_SDL textInput;
    ContextPointer context;
    Model model;
    WindowChrome chrome;
    Tooltip tooltip;
    Hotkeys hotkeys;
    TextMenu textMenu;
    std::move_only_function<void()> close;
    Uint64 lastUpdateNs = 0;
    std::optional<Rml::Vector2i> overlaySize;
};

}
