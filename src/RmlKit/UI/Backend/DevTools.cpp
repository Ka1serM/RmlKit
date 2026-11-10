#include "RmlKit/UI/Backend/DevTools.h"

#include <RmlUi/Core/Factory.h>
#include <RmlUi/Debugger.h>

#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Backend/Router.h"

namespace RmlKit::UI {

DevTools::DevTools(Rml::Context& context, Router& router) : router(router) {
    if (!Rml::Debugger::Initialise(&context))
        fatal("RmlUi cannot initialize the debugger");
}

DevTools::~DevTools() {
    Rml::Debugger::Shutdown();
}

bool DevTools::handle(const SDL_Event& event) {
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
        return false;
    switch (event.key.key) {
    case SDLK_F5:
        Rml::Factory::ClearStyleSheetCache();
        Rml::Factory::ClearTemplateCache();
        router.reload();
        return true;
    case SDLK_F8:
        Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
        return true;
    default:
        return false;
    }
}

}
