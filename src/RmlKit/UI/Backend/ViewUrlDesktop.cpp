#include "RmlKit/UI/Backend/ViewUrl.h"

namespace RmlKit::UI {

ViewUrl::ViewUrl(Router&) {}

ViewUrl::~ViewUrl() = default;

Rml::String ViewUrl::getView() const {
    return {};
}

void ViewUrl::show(const Rml::String&) {}

}
