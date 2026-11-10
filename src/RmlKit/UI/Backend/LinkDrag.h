#pragma once

#include <memory>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

namespace Rml {
class Context;
class Element;
}

namespace RmlKit::UI {

class Model;

class LinkDrag {
public:
    LinkDrag(Rml::Context& context, Model& model, SDL_Window* window);
    ~LinkDrag();
    LinkDrag(const LinkDrag&) = delete;
    LinkDrag& operator=(const LinkDrag&) = delete;

    void handle(const SDL_Event& event);

private:
    Rml::Element* linkAt(float x, float y) const;

    struct Native;

    Rml::Context& context;
    Model& model;
    SDL_Window* window;
    std::unique_ptr<Native> native;
};

}
