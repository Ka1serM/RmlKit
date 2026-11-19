#include "RmlKit/UI/Controls/ColorPickerElement.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

namespace RmlKit::UI {

namespace {

using Rgb = std::array<float, 3>;

Rgb rgbOf(const float hue, const float saturation, const float value) {
    const auto channel = [&](const float n) {
        const float k = std::fmod(n + hue / 60.0f, 6.0f);
        return value - value * saturation * std::max(0.0f, std::min({k, 4.0f - k, 1.0f}));
    };
    return {channel(5.0f), channel(3.0f), channel(1.0f)};
}

Rml::String hexOf(const Rgb& rgb) {
    const auto byte = [](const float channel) { return static_cast<int>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f)); };
    return std::format("#{:02x}{:02x}{:02x}", byte(rgb[0]), byte(rgb[1]), byte(rgb[2]));
}

std::optional<Rgb> parseHex(Rml::String text) {
    if (text.starts_with('#'))
        text.erase(0, 1);
    if (text.size() == 3)
        text = {text[0], text[0], text[1], text[1], text[2], text[2]};
    if (text.size() != 6 || text.find_first_not_of("0123456789abcdefABCDEF") != Rml::String::npos)
        return std::nullopt;
    Rgb rgb;
    for (int i = 0; i < 3; ++i)
        rgb[i] = static_cast<float>(std::stoi(text.substr(i * 2, 2), nullptr, 16)) / 255.0f;
    return rgb;
}

Rml::String percent(const float fraction) {
    return std::format("{}%", std::clamp(fraction, 0.0f, 1.0f) * 100.0f);
}

}

ColorPickerElement::ColorPickerElement(const Rml::String& tag) : Rml::Element(tag) {}

ColorPickerElement::~ColorPickerElement() {
    if (field)
        field->RemoveEventListener(Rml::EventId::Mousedown, this);
    if (field)
        field->RemoveEventListener(Rml::EventId::Drag, this);
    if (hueBar)
        hueBar->RemoveEventListener(Rml::EventId::Mousedown, this);
    if (hueBar)
        hueBar->RemoveEventListener(Rml::EventId::Drag, this);
    if (hexField)
        hexField->RemoveEventListener(Rml::EventId::Change, this);
}

void ColorPickerElement::build() {
    Rml::ElementDocument* document = GetOwnerDocument();
    const auto add = [&](Rml::Element& parent, const char* tag, const char* name) {
        Rml::Element* child = parent.AppendChild(document->CreateElement(tag));
        child->SetClass(name, true);
        return child;
    };
    field = add(*this, "div", "color-field");
    fieldThumb = add(*field, "div", "color-field-thumb");
    hueBar = add(*this, "div", "color-hue");
    hueThumb = add(*hueBar, "div", "color-hue-thumb");
    Rml::Element* row = add(*this, "div", "color-row");
    swatch = add(*row, "div", "color-swatch");
    hexField = add(*row, "input", "color-hex");
    hexField->SetAttribute("type", "text");
    hexField->SetAttribute("maxlength", 7);
    field->AddEventListener(Rml::EventId::Mousedown, this);
    field->AddEventListener(Rml::EventId::Drag, this);
    hueBar->AddEventListener(Rml::EventId::Mousedown, this);
    hueBar->AddEventListener(Rml::EventId::Drag, this);
    hexField->AddEventListener(Rml::EventId::Change, this);
    show();
}

void ColorPickerElement::OnUpdate() {
    if (!field && GetOwnerDocument())
        build();
}

void ColorPickerElement::OnAttributeChange(const Rml::ElementAttributes& changed) {
    Rml::Element::OnAttributeChange(changed);
    if (changed.find("value") != changed.end())
        setHex(GetAttribute<Rml::String>("value", ""));
}

//! A grey keeps the hue it had, so dragging through black or white does not lose it.
void ColorPickerElement::setHex(const Rml::String& text) {
    const std::optional<Rgb> rgb = parseHex(text);
    if (!rgb || hexOf(*rgb) == hex)
        return;
    const auto [r, g, b] = *rgb;
    const float high = std::max({r, g, b});
    const float range = high - std::min({r, g, b});
    if (range > 0.0f) {
        if (high == r)
            hsv.hue = 60.0f * std::fmod((g - b) / range + 6.0f, 6.0f);
        else if (high == g)
            hsv.hue = 60.0f * ((b - r) / range + 2.0f);
        else
            hsv.hue = 60.0f * ((r - g) / range + 4.0f);
    }
    hsv.saturation = high > 0.0f ? range / high : 0.0f;
    hsv.value = high;
    hex = hexOf(*rgb);
    show();
}

void ColorPickerElement::ProcessEvent(Rml::Event& event) {
    Rml::Element* current = event.GetCurrentElement();
    if (current == hexField) {
        event.StopPropagation();
        const Rml::String before = hex;
        setHex(event.GetParameter<Rml::String>("value", ""));
        if (hex != before)
            commit();
        return;
    }
    if (current == field || current == hueBar)
        pick(*current, event);
}

void ColorPickerElement::pick(Rml::Element& area, const Rml::Event& event) {
    const Rml::Vector2f mouse = event.GetUnprojectedMouseScreenPos();
    const Rml::Vector2f origin = area.GetAbsoluteOffset(Rml::BoxArea::Content);
    const Rml::Vector2f size = area.GetBox().GetSize(Rml::BoxArea::Content);
    if (size.x <= 0.0f || size.y <= 0.0f)
        return;
    const float x = std::clamp((mouse.x - origin.x) / size.x, 0.0f, 1.0f);
    const float y = std::clamp((mouse.y - origin.y) / size.y, 0.0f, 1.0f);
    if (&area == hueBar)
        hsv.hue = x * 360.0f;
    else {
        hsv.saturation = x;
        hsv.value = 1.0f - y;
    }
    hex = hexOf(rgbOf(hsv.hue, hsv.saturation, hsv.value));
    show();
    commit();
}

void ColorPickerElement::commit() {
    SetAttribute("value", hex);
    DispatchEvent(Rml::EventId::Change, {{"value", Rml::Variant(hex)}});
}

void ColorPickerElement::show() {
    if (!field)
        return;
    field->SetProperty("background-color", hexOf(rgbOf(hsv.hue, 1.0f, 1.0f)));
    fieldThumb->SetProperty("left", percent(hsv.saturation));
    fieldThumb->SetProperty("top", percent(1.0f - hsv.value));
    fieldThumb->SetProperty("background-color", hex);
    hueThumb->SetProperty("left", percent(hsv.hue / 360.0f));
    swatch->SetProperty("background-color", hex);
    if (!hexField->IsPseudoClassSet("focus"))
        hexField->SetAttribute("value", hex);
}

}
