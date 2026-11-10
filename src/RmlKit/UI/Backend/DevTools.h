#pragma once

#include <SDL3/SDL_events.h>

namespace Rml {
class Context;
}

namespace RmlKit::UI {

class Router;

class DevTools {
public:
    DevTools(Rml::Context& context, Router& router);
    ~DevTools();
    DevTools(const DevTools&) = delete;
    DevTools& operator=(const DevTools&) = delete;

    bool handle(const SDL_Event& event);

private:
    Router& router;
};

}
