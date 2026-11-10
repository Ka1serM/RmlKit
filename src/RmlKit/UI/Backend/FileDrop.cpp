#include "RmlKit/UI/Backend/FileDrop.h"

#include <RmlUi/Core/Context.h>

namespace RmlKit::UI {

FileDrop::FileDrop(Rml::Context& context, SDL_Window* window) : context(context), window(window) {}

void FileDrop::handle(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_DROP_POSITION:
        hover(targetAt(event.drop.x, event.drop.y));
        return;
    case SDL_EVENT_DROP_FILE: {
        Rml::Element* target = targetAt(event.drop.x, event.drop.y);
        if (!target)
            return;
        const Rml::Dictionary parameters{{"path", Rml::Variant(Rml::String(event.drop.data))}};
        target->DispatchEvent("filedrop", parameters);
        return;
    }
    case SDL_EVENT_DROP_COMPLETE:
        hover(nullptr);
        return;
    default:
        return;
    }
}

Rml::Element* FileDrop::targetAt(const float x, const float y) const {
    const float density = SDL_GetWindowPixelDensity(window);
    for (Rml::Element* element = context.GetElementAtPoint({x * density, y * density}); element; element = element->GetParentNode())
        if (element->IsClassSet("file-drop"))
            return element;
    return nullptr;
}

void FileDrop::hover(Rml::Element* target) {
    if (hovered.get() == target)
        return;
    if (hovered)
        hovered->SetClass("file-over", false);
    hovered = target ? target->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
    if (target)
        target->SetClass("file-over", true);
}

}
