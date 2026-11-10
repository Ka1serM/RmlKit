#include "RmlKit/UI/Backend/LinkDrag.h"

#include <RmlUi/Core/Element.h>

extern "C" void rmlkit_show_link_drag(float x, float y, float width, float height, const char* link);
extern "C" void rmlkit_hide_link_drag();

namespace RmlKit::UI {

struct LinkDrag::Native {};

LinkDrag::LinkDrag(Rml::Context& context, Model& model, SDL_Window* window)
    : context(context), model(model), window(window), native(std::make_unique<Native>()) {}

LinkDrag::~LinkDrag() = default;

void LinkDrag::handle(const SDL_Event& event) {
    if (event.type != SDL_EVENT_MOUSE_MOTION)
        return;
    Rml::Element* element = linkAt(event.motion.x, event.motion.y);
    if (!element) {
        rmlkit_hide_link_drag();
        return;
    }
    const float density = SDL_GetWindowPixelDensity(window);
    const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border) / density;
    const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border) / density;
    rmlkit_show_link_drag(offset.x, offset.y, size.x, size.y, element->GetAttribute<Rml::String>("href", "").c_str());
}

}
