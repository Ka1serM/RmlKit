#include "RmlKit/UI/Backend/LinkDrag.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>

namespace RmlKit::UI {

Rml::Element* LinkDrag::linkAt(const float x, const float y) const {
    const float density = SDL_GetWindowPixelDensity(window);
    for (Rml::Element* element = context.GetElementAtPoint({x * density, y * density}); element; element = element->GetParentNode())
        if (element->IsClassSet("link-drag"))
            return element;
    return nullptr;
}

}
