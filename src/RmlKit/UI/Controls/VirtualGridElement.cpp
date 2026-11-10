#include "RmlKit/UI/Controls/VirtualGridElement.h"

#include <cstdlib>

#include <RmlUi/Core/Box.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Event.h>

namespace RmlKit::UI {

VirtualGridElement::VirtualGridElement(const Rml::String& tag) : Rml::Element(tag) {
    AddEventListener(Rml::EventId::Scroll, this);
}

VirtualGridElement::~VirtualGridElement() {
    RemoveEventListener(Rml::EventId::Scroll, this);
}

void VirtualGridElement::OnLayout() {
    const float scrollTop = GetScrollTop();
    if (following && atEnd)
        scrollToEnd();
    //! RmlUi clamps the scroll when the content shrinks or while the grid is hidden, and sends no scroll event for it.
    const bool moved = Rml::Vector2f(GetClientWidth(), GetClientHeight()) != dispatchedClient || GetScrollTop() != dispatchedScroll;
    if (moved)
        dispatchViewport();
    if (!moved && GetScrollTop() == scrollTop)
        return;
    if (Rml::Context* context = GetContext())
        context->RequestNextUpdate(0);
}

void VirtualGridElement::OnAttributeChange(const Rml::ElementAttributes& changed) {
    Rml::Element::OnAttributeChange(changed);
    if (const auto scrollTo = changed.find("scroll-to"); scrollTo != changed.end())
        scrollToRequest(scrollTo->second.Get<Rml::String>());
    const auto follow = changed.find("follow");
    if (follow == changed.end())
        return;
    const bool wasFollowing = following;
    following = follow->second.Get<bool>();
    if (!following || wasFollowing)
        return;
    atEnd = true;
    scrollToEnd();
}

void VirtualGridElement::ProcessEvent(Rml::Event& event) {
    if (event.GetTargetElement() != this)
        return;
    atEnd = GetScrollTop() + GetClientHeight() >= GetScrollHeight() - 1;
    dispatchViewport();
}

void VirtualGridElement::dispatchViewport() {
    const Rml::Context* context = GetContext();
    if (!context)
        return;
    const float dp = context->GetDensityIndependentPixelRatio();
    const Rml::Box& box = GetBox();
    const float paddingTop = box.GetEdge(Rml::BoxArea::Padding, Rml::BoxEdge::Top);
    const float paddingX = box.GetEdge(Rml::BoxArea::Padding, Rml::BoxEdge::Left) + box.GetEdge(Rml::BoxArea::Padding, Rml::BoxEdge::Right);
    dispatchedClient = {GetClientWidth(), GetClientHeight()};
    dispatchedScroll = GetScrollTop();
    Rml::Dictionary parameters;
    parameters["scroll_top"] = (GetScrollTop() - paddingTop) / dp;
    parameters["width"] = (GetClientWidth() - paddingX) / dp;
    parameters["height"] = GetClientHeight() / dp;
    parameters["at_end"] = atEnd;
    DispatchEvent("viewport", parameters);
}

void VirtualGridElement::scrollToRequest(const Rml::String& request) {
    const auto separator = request.find(' ');
    const Rml::Context* context = GetContext();
    if (separator == Rml::String::npos || !context)
        return;
    const float top = std::strtof(request.c_str() + separator + 1, nullptr);
    const float paddingTop = GetBox().GetEdge(Rml::BoxArea::Padding, Rml::BoxEdge::Top);
    SetScrollTop(top * context->GetDensityIndependentPixelRatio() + paddingTop);
}

void VirtualGridElement::scrollToEnd() {
    SetScrollTop(GetScrollHeight() - GetClientHeight());
}

}
