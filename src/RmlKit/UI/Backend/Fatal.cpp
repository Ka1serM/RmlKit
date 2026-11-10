#include "RmlKit/UI/Backend/Fatal.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <SDL3/SDL_messagebox.h>

namespace RmlKit::UI {

void fatal(const std::string_view message) {
    const std::string text(message);
    std::fprintf(stderr, "%s: %s\n", PRODUCT_NAME, text.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, PRODUCT_NAME, text.c_str(), nullptr);
    std::_Exit(EXIT_FAILURE);
}

}
