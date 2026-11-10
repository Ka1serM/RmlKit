#include "RmlKit/UI/Backend/Tooltip.h"

#include <algorithm>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementText.h>

namespace RmlKit::UI {

namespace {

constexpr const char* hostAttribute = "tooltip";
constexpr const char* keyAttribute = "tooltip-key";
constexpr float cursorOffsetXDp = 12.0f;
constexpr float cursorOffsetYDp = 20.0f;
constexpr float gapDp = 6.0f;
constexpr float marginDp = 4.0f;

//! A tooltip only names an element that shows no label, such as an icon button or a sidebar that hides its labels when narrow.
bool showsText(const Rml::Element& element) {
    if (const auto* text = dynamic_cast<const Rml::ElementText*>(&element))
        return element.IsVisible(true) && text->GetText().find_first_not_of(" \t\n") != Rml::String::npos;
    for (int i = 0; i < element.GetNumChildren(); ++i)
        if (showsText(*element.GetChild(i)))
            return true;
    return false;
}

}

Tooltip::Tooltip(Rml::Context& context) : context(context) {
    context.AddEventListener("mousemove", this);
    context.AddEventListener("mouseout", this);
    context.AddEventListener("mousedown", this);
    context.AddEventListener("mousescroll", this);
}

Tooltip::~Tooltip() {
    context.RemoveEventListener("mousemove", this);
    context.RemoveEventListener("mouseout", this);
    context.RemoveEventListener("mousedown", this);
    context.RemoveEventListener("mousescroll", this);
}

void Tooltip::ProcessEvent(Rml::Event& event) {
    if (event.GetId() != Rml::EventId::Mousemove) {
        hide();
        return;
    }
    Rml::Element* host = hostOf(event.GetTargetElement());
    if (!host || showsText(*host)) {
        hide();
        return;
    }
    const Rml::Vector2f cursor(event.GetParameter<float>("mouse_x", 0.0f), event.GetParameter<float>("mouse_y", 0.0f));
    show(*host, cursor);
}

Rml::Element* Tooltip::hostOf(Rml::Element* element) {
    for (; element; element = element->GetParentNode())
        if (element->HasAttribute(hostAttribute))
            return element;
    return nullptr;
}

Rml::String Tooltip::markupOf(const Rml::Element& host) {
    const Rml::String text = host.GetAttribute<Rml::String>(hostAttribute, "");
    if (text.empty())
        return {};
    const Rml::String key = host.GetAttribute<Rml::String>(keyAttribute, "");
    const Rml::String label = "<span>" + text + "</span>";
    return key.empty() ? label : label + "<span class=\"kbd\">" + key + "</span>";
}

Rml::Element* Tooltip::bubble() const {
    Rml::ElementDocument* document = context.GetDocument("main");
    return document ? document->GetElementById("tooltip") : nullptr;
}

void Tooltip::show(const Rml::Element& host, Rml::Vector2f cursor) {
    Rml::Element* element = bubble();
    if (!element)
        return;
    const Rml::String markup = markupOf(host);
    if (markup.empty()) {
        hide();
        return;
    }
    if (markup != shownMarkup || !element->IsClassSet("shown")) {
        shownMarkup = markup;
        element->SetInnerRML(markup);
        element->SetClass("shown", true);
        element->GetOwnerDocument()->UpdateDocument();
    }

    const float dp = context.GetDensityIndependentPixelRatio();
    const Rml::Vector2f viewport(context.GetDimensions());
    const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
    const float margin = marginDp * dp;

    const float below = cursor.y + cursorOffsetYDp * dp;
    const float top = below + size.y + margin <= viewport.y ? below : cursor.y - gapDp * dp - size.y;
    const float left = std::clamp(cursor.x + cursorOffsetXDp * dp, margin, std::max(margin, viewport.x - size.x - margin));
    element->SetProperty(Rml::PropertyId::Left, Rml::Property(left, Rml::Unit::PX));
    element->SetProperty(Rml::PropertyId::Top, Rml::Property(std::max(margin, top), Rml::Unit::PX));
}

void Tooltip::hide() {
    if (Rml::Element* element = bubble())
        element->SetClass("shown", false);
}

}
