#include "RmlKit/UI/Backend/ViewUrl.h"

#include <cstdlib>
#include <memory>

#include <emscripten/emscripten.h>

#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Backend/Router.h"

extern "C" char* rmlkit_read_url_view();
extern "C" void rmlkit_push_url_view(const char* view);
extern "C" void rmlkit_listen_url_view();
extern "C" void rmlkit_unlisten_url_view();

namespace RmlKit::UI {

namespace {

Router* current = nullptr;

}

ViewUrl::ViewUrl(Router& router) {
    if (current)
        fatal("The page has a ViewUrl already");
    current = &router;
    rmlkit_listen_url_view();
}

ViewUrl::~ViewUrl() {
    rmlkit_unlisten_url_view();
    current = nullptr;
}

Rml::String ViewUrl::getView() const {
    const std::unique_ptr<char, decltype(&std::free)> view(rmlkit_read_url_view(), &std::free);
    return view.get();
}

void ViewUrl::show(const Rml::String& view) {
    rmlkit_push_url_view(view.c_str());
}

}

extern "C" EMSCRIPTEN_KEEPALIVE void rmlkit_url_view(const char* view) {
    if (RmlKit::UI::current)
        RmlKit::UI::current->showFromUrl(view);
}
