#include "RmlKit/UI/Controls/SplitterElement.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Event.h>

namespace RmlKit::UI {

SplitterElement::SplitterElement(const Rml::String& tag) : Rml::Element(tag) {}

Rml::Element* SplitterElement::target() {
    return GetAttribute("target-before", false) ? GetPreviousSibling() : GetNextSibling();
}

float SplitterElement::pointer(const Rml::Event& event) const {
    const Rml::Vector2f mouse = event.GetUnprojectedMouseScreenPos();
    return (IsClassSet("vertical") ? mouse.x : mouse.y) / GetContext()->GetDensityIndependentPixelRatio();
}

//! The target takes at most what its container leaves beside the handle, so it never pushes the other items out.
float SplitterElement::clamped(const float wanted) {
    const bool vertical = IsClassSet("vertical");
    float upper = GetAttribute("max-size", 600.0f);
    float lower = GetAttribute("min-size", 120.0f);
    if (lower > upper)
        std::swap(lower, upper);
    if (Rml::Element* container = GetParentNode()) {
        const float ratio = GetContext()->GetDensityIndependentPixelRatio();
        const Rml::Vector2f containerSize = container->GetBox().GetSize();
        const Rml::Vector2f handleSize = GetBox().GetSize(Rml::BoxArea::Margin);
        const float available = ((vertical ? containerSize.x : containerSize.y) - (vertical ? handleSize.x : handleSize.y)) / ratio;
        if (available > 0)
            upper = std::min(upper, available);
    }
    return std::clamp(wanted, std::min(lower, upper), upper);
}

//! A hidden handle cannot be dragged, so it leaves the target's size to the stylesheet.
void SplitterElement::apply() {
    Rml::Element* panel = target();
    if (!panel)
        return;
    if (GetComputedValues().display() == Rml::Style::Display::None) {
        if (appliedSize >= 0) {
            panel->RemoveProperty("flex");
            appliedSize = -1;
        }
        return;
    }
    const float rounded = std::round(clamped(size));
    if (rounded == appliedSize)
        return;
    appliedSize = rounded;
    panel->SetProperty("flex", std::format("0 0 {}dp", rounded));
}

//! The limit applies on every update, so a smaller window shrinks the target too.
void SplitterElement::OnUpdate() {
    if (size < 0)
        size = GetAttribute("default-size", 300.0f);
    apply();
}

void SplitterElement::ProcessDefaultAction(Rml::Event& event) {
    Rml::Element::ProcessDefaultAction(event);
    if (event.GetTargetElement() != this)
        return;
    if (event == Rml::EventId::Dragstart) {
        dragStartPointer = pointer(event);
        dragStartSize = clamped(size);
    } else if (event == Rml::EventId::Drag) {
        //! The target follows the handle, so a drag toward it shrinks it.
        const float direction = GetAttribute("target-before", false) ? 1.0f : -1.0f;
        size = clamped(dragStartSize + direction * (pointer(event) - dragStartPointer));
        apply();
    }
}

}
