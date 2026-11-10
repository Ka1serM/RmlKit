#pragma once

#include <optional>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Geometry.h>

namespace RmlKit::UI {

class SpinnerRing {
public:
    SpinnerRing(Rml::RenderManager& renderManager, float diameter, Rml::ColourbPremultiplied colour);

    void render(Rml::Element& element, Rml::Vector2f center) const;

private:
    Rml::Geometry geometry;
};

class SpinnerElement final : public Rml::Element {
public:
    explicit SpinnerElement(const Rml::String& tag) : Rml::Element(tag) {}

protected:
    void OnRender() override;
    void OnResize() override;
    void OnPropertyChange(const Rml::PropertyIdSet& changedProperties) override;

private:
    std::optional<SpinnerRing> ring;
};

}
