#include "RmlKit/UI/Controls/Maximizer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Property.h>

namespace RmlKit::UI {

Maximizer::~Maximizer() {
    if (keyContext)
        keyContext->RemoveEventListener("keydown", &listener);
}

void Maximizer::set(const bool maximized) {
    element.SetClass("maximized", maximized);
    if (maximized) {
        if (!keyContext) {
            keyContext = element.GetContext();
            if (keyContext)
                keyContext->AddEventListener("keydown", &listener);
        }
        update();
        return;
    }
    if (keyContext)
        keyContext->RemoveEventListener("keydown", &listener);
    keyContext = nullptr;
    for (const Rml::PropertyId property :
         {Rml::PropertyId::Position, Rml::PropertyId::Left, Rml::PropertyId::Top, Rml::PropertyId::Width, Rml::PropertyId::Height})
        element.RemoveProperty(property);
    offset.reset();
    size.reset();
}

void Maximizer::update() {
    Rml::Context* context = element.GetContext();
    if (!context || !isMaximized())
        return;
    //! RmlUi places a fixed element like an absolute one, in its nearest positioned ancestor.
    if (!offset)
        element.SetProperty("position", "absolute");
    Rml::Element* containingBlock = element.GetOffsetParent();
    const Rml::Vector2f origin = containingBlock ? containingBlock->GetAbsoluteOffset(Rml::BoxArea::Padding) : Rml::Vector2f(0, 0);
    Rml::Element* area = element.Closest(".maximize-area");
    const Rml::Vector2f areaOrigin = area ? area->GetAbsoluteOffset(Rml::BoxArea::Border) : Rml::Vector2f(0, 0);
    const Rml::Vector2f areaSize = area ? area->GetBox().GetSize(Rml::BoxArea::Border) : Rml::Vector2f(context->GetDimensions());
    const Rml::Vector2f areaOffset = areaOrigin - origin;
    if (areaOffset != offset) {
        offset = areaOffset;
        element.SetProperty(Rml::PropertyId::Left, Rml::Property(areaOffset.x, Rml::Unit::PX));
        element.SetProperty(Rml::PropertyId::Top, Rml::Property(areaOffset.y, Rml::Unit::PX));
    }
    if (areaSize != size) {
        size = areaSize;
        element.SetProperty(Rml::PropertyId::Width, Rml::Property(areaSize.x, Rml::Unit::PX));
        element.SetProperty(Rml::PropertyId::Height, Rml::Property(areaSize.y, Rml::Unit::PX));
    }
}

bool Maximizer::handleKey(Rml::Event& event) {
    if (event.GetId() != Rml::EventId::Keydown)
        return false;
    if (event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN) != Rml::Input::KI_ESCAPE)
        return true;
    const bool visible = element.IsVisible(true);
    set(false);
    if (visible)
        event.StopPropagation();
    return true;
}

}
