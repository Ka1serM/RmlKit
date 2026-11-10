#pragma once

#include <RmlUi/Core/Decorator.h>

namespace RmlKit::UI {

class Renderer;

class TextureLoadingDecorator final : public Rml::Decorator {
public:
    explicit TextureLoadingDecorator(const Renderer& renderer) : renderer(renderer) {}

    Rml::DecoratorDataHandle GenerateElementData(Rml::Element* element, Rml::BoxArea paintArea) const override;
    void ReleaseElementData(Rml::DecoratorDataHandle elementData) const override;
    void RenderElement(Rml::Element* element, Rml::DecoratorDataHandle elementData) const override;

private:
    const Renderer& renderer;
};

class TextureLoadingDecoratorInstancer final : public Rml::DecoratorInstancer {
public:
    explicit TextureLoadingDecoratorInstancer(const Renderer& renderer) : renderer(renderer) {}

    Rml::SharedPtr<Rml::Decorator> InstanceDecorator(const Rml::String& name, const Rml::PropertyDictionary& properties,
                                                     const Rml::DecoratorInstancerInterface& instancerInterface) override;

private:
    const Renderer& renderer;
};

}
