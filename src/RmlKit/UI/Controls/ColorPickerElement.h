#pragma once

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>

namespace RmlKit::UI {

//! <color-picker value="#rrggbb">: a saturation and value field, a hue bar and a hex field; it sends change with the new value, so data-value binds it.
class ColorPickerElement final : public Rml::Element, private Rml::EventListener {
public:
    explicit ColorPickerElement(const Rml::String& tag);
    ~ColorPickerElement() override;

protected:
    void OnUpdate() override;
    void OnAttributeChange(const Rml::ElementAttributes& changed) override;

private:
    struct Hsv {
        float hue = 0.0f;
        float saturation = 0.0f;
        float value = 1.0f;
    };

    void ProcessEvent(Rml::Event& event) override;
    void build();
    void pick(Rml::Element& area, const Rml::Event& event);
    void setHex(const Rml::String& hex);
    void commit();
    void show();

    Hsv hsv;
    Rml::String hex = "#ffffff";
    Rml::Element* field = nullptr;
    Rml::Element* fieldThumb = nullptr;
    Rml::Element* hueBar = nullptr;
    Rml::Element* hueThumb = nullptr;
    Rml::Element* swatch = nullptr;
    Rml::Element* hexField = nullptr;
};

}
