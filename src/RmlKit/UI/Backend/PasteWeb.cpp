#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <SDL3/SDL_events.h>
#include <emscripten/emscripten.h>

//! SDL cannot read the clipboard in a browser: the page (web/shell.html) sends a paste into the canvas here.
extern "C" EMSCRIPTEN_KEEPALIVE void rmlkit_paste(const char* text) {
    Rml::Context* context = Rml::GetContext("main");
    if (!context)
        return;
    context->ProcessTextInput(text);
    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    SDL_PushEvent(&event);
}
