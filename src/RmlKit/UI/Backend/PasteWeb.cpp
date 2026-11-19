#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Input.h>
#include <SDL3/SDL_events.h>
#include <emscripten/emscripten.h>

#include "RmlKit/UI/Backend/Hotkeys.h"

//! web/shell.html sends each browser paste here, as SDL cannot read the clipboard: a text field takes the text, and elsewhere it runs the app's Mod+V hotkey.
extern "C" EMSCRIPTEN_KEEPALIVE void rmlkit_paste(const char* text) {
    Rml::Context* context = Rml::GetContext("main");
    if (!context)
        return;
    if (RmlKit::UI::isTextField(context->GetFocusElement())) {
        if (*text != '\0')
            context->ProcessTextInput(text);
    } else {
        const int modifiers = RmlKit::UI::parseHotkey("Mod+V")->modifiers;
        context->ProcessKeyDown(Rml::Input::KI_V, modifiers);
        context->ProcessKeyUp(Rml::Input::KI_V, modifiers);
    }
    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    SDL_PushEvent(&event);
}
