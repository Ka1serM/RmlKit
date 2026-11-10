#include <SDL3/SDL_clipboard.h>

#include "RmlKit/UI/Backend/LogRoute.h"

//! NOLINTNEXTLINE(readability-identifier-naming) The name is an emscripten JS library import from web/library.js.
extern "C" void rmlkit_copy_text(const char* text);

namespace RmlKit::UI {

void LogSystemInterface::SetClipboardText(const Rml::String& text) {
    //! SDL keeps it too, so a paste of RmlUi's own Ctrl+V reads the last copy.
    SDL_SetClipboardText(text.c_str());
    rmlkit_copy_text(text.c_str());
}

}
