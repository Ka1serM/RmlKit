#include "RmlKit/UI/Controls/TextureLoadingDecorator.h"

#include <algorithm>
#include <memory>

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>

#include "RmlKit/UI/Backend/Renderer.h"
#include "RmlKit/UI/Controls/SpinnerElement.h"

namespace RmlKit::UI {

namespace {

struct LoadingSpinner {
    SpinnerRing ring;
    Rml::Vector2f center;
};

}

Rml::DecoratorDataHandle TextureLoadingDecorator::GenerateElementData(Rml::Element* const element, Rml::BoxArea) const {
    Rml::RenderManager* const renderManager = element->GetRenderManager();
    if (!renderManager)
        return INVALID_DECORATORDATAHANDLE;
    const Rml::Box& box = element->GetBox();
    const Rml::Vector2f size = box.GetSize(Rml::BoxArea::Content);
    const float diameter = std::min({20.0f * element->GetContext()->GetDensityIndependentPixelRatio(), size.x, size.y});
    if (diameter <= 0.0f)
        return INVALID_DECORATORDATAHANDLE;
    const Rml::ComputedValues& computed = element->GetComputedValues();
    auto spinner = std::make_unique<LoadingSpinner>(SpinnerRing(*renderManager, diameter, computed.color().ToPremultiplied(computed.opacity())),
                                                    box.GetPosition(Rml::BoxArea::Content) + size / 2.0f);
    return reinterpret_cast<Rml::DecoratorDataHandle>(spinner.release());
}

void TextureLoadingDecorator::ReleaseElementData(const Rml::DecoratorDataHandle elementData) const {
    std::unique_ptr<LoadingSpinner>(reinterpret_cast<LoadingSpinner*>(elementData));
}

void TextureLoadingDecorator::RenderElement(Rml::Element* const element, const Rml::DecoratorDataHandle elementData) const {
    const Rml::Variant* const source = element->GetAttribute("src");
    if (!source || !renderer.readsExternalTexture(source->GetReference<Rml::String>()))
        return;
    const auto& spinner = *reinterpret_cast<const LoadingSpinner*>(elementData);
    spinner.ring.render(*element, element->GetAbsoluteOffset(Rml::BoxArea::Border) + spinner.center);
}

Rml::SharedPtr<Rml::Decorator> TextureLoadingDecoratorInstancer::InstanceDecorator(const Rml::String&, const Rml::PropertyDictionary&,
                                                                                   const Rml::DecoratorInstancerInterface&) {
    return std::make_shared<TextureLoadingDecorator>(renderer);
}

}
