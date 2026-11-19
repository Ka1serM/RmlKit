#pragma once

#include <functional>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include "RmlKit/Core/StdGaps.h"

namespace Rml {
class Context;
}

namespace RmlKit::UI {

class Model;

class WindowChrome {
public:
    WindowChrome(Rml::Context& context, Model& model, SDL_Window* window, std::move_only_function<void()> close);
    ~WindowChrome();
    WindowChrome(const WindowChrome&) = delete;
    WindowChrome& operator=(const WindowChrome&) = delete;

    void handle(const SDL_Event& event);
    bool isTitleBarAt(float x, float y) const { return regionAt(x, y) == Region::TitleBar; }

private:
    void attach();
    void detach();
    static SDL_HitTestResult SDLCALL hitTest(SDL_Window* window, const SDL_Point* area, void* chrome);
    SDL_HitTestResult hitTestAt(SDL_Point area) const;
    enum class Region { None, Background, TitleBar };
    Region regionAt(float x, float y) const;

    void closeWindow(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&);
    void minimizeWindow(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&);
    void toggleMaximized(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&);
    void pressWindowButton(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);
    void toggleMaximized();
    void showMaximized();

    Rml::Context& context;
    Model& model;
    SDL_Window* window;
    std::move_only_function<void()> close;
    const bool wayland;
    SDL_HitTestResult lastHitTest = SDL_HITTEST_NORMAL;
    bool lastHitTitleBar = false;
    Uint64 lastDragPress = 0;
    bool maximizeAfterMove = false;
    Uint64 lastMaximizeToggle = 0;
};

}
