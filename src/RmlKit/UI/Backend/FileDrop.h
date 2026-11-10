#pragma once

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ObserverPtr.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

namespace Rml {
class Context;
}

namespace RmlKit::UI {

class FileDrop {
public:
    FileDrop(Rml::Context& context, SDL_Window* window);
    FileDrop(const FileDrop&) = delete;
    FileDrop& operator=(const FileDrop&) = delete;

    void handle(const SDL_Event& event);

private:
    Rml::Element* targetAt(float x, float y) const;
    void hover(Rml::Element* target);

    Rml::Context& context;
    SDL_Window* window;
    Rml::ObserverPtr<Rml::Element> hovered;
};

}
