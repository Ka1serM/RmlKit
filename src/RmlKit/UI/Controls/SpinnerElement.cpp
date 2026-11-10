#include "RmlKit/UI/Controls/SpinnerElement.h"

#include <algorithm>
#include <cmath>

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Math.h>
#include <RmlUi/Core/Mesh.h>
#include <RmlUi/Core/PropertyIdSet.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi/Core/SystemInterface.h>

namespace RmlKit::UI {

namespace {

Rml::Mesh makeRing(const float diameter, const Rml::ColourbPremultiplied colour) {
    constexpr int segments = 24;
    constexpr float sweep = 1.5f * Rml::Math::RMLUI_PI;
    const float outer = diameter / 2.0f;
    const float inner = outer - diameter / 10.0f;
    Rml::Mesh mesh;
    mesh.vertices.reserve(2 * (segments + 1));
    mesh.indices.reserve(6 * segments);
    for (int i = 0; i <= segments; ++i) {
        const float angle = sweep * static_cast<float>(i) / segments;
        const Rml::Vector2f direction(std::cos(angle), std::sin(angle));
        mesh.vertices.push_back({direction * outer, colour});
        mesh.vertices.push_back({direction * inner, colour});
    }
    for (int i = 0; i < segments; ++i) {
        const int first = 2 * i;
        mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first + 2, first + 1, first + 3});
    }
    return mesh;
}

}

SpinnerRing::SpinnerRing(Rml::RenderManager& renderManager, const float diameter, const Rml::ColourbPremultiplied colour)
    : geometry(renderManager.MakeGeometry(makeRing(diameter, colour))) {}

void SpinnerRing::render(Rml::Element& element, const Rml::Vector2f center) const {
    element.GetContext()->RequestNextUpdate(0.0);
    const double turns = std::fmod(Rml::GetSystemInterface()->GetElapsedTime(), 1.0);
    Rml::RenderManager& renderManager = *element.GetRenderManager();
    const Rml::Matrix4f elementTransform = renderManager.GetState().transform;
    const Rml::Matrix4f turned =
        elementTransform * Rml::Matrix4f::Translate(center.x, center.y, 0.0f) * Rml::Matrix4f::RotateZ(static_cast<float>(turns) * 2.0f * Rml::Math::RMLUI_PI);
    renderManager.SetTransform(&turned);
    geometry.Render({});
    renderManager.SetTransform(&elementTransform);
}

void SpinnerElement::OnRender() {
    const Rml::Vector2f size = GetBox().GetSize(Rml::BoxArea::Content);
    const float diameter = std::min(size.x, size.y);
    if (diameter <= 0.0f)
        return;
    if (!ring) {
        const Rml::ComputedValues& computed = GetComputedValues();
        ring.emplace(*GetRenderManager(), diameter, computed.image_color().ToPremultiplied(computed.opacity()));
    }
    ring->render(*this, GetAbsoluteOffset(Rml::BoxArea::Content) + size / 2.0f);
}

void SpinnerElement::OnResize() {
    ring.reset();
}

void SpinnerElement::OnPropertyChange(const Rml::PropertyIdSet& changedProperties) {
    Rml::Element::OnPropertyChange(changedProperties);
    if (changedProperties.Contains(Rml::PropertyId::ImageColor) || changedProperties.Contains(Rml::PropertyId::Opacity))
        ring.reset();
}

}
