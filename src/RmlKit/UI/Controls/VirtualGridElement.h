#pragma once

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>

namespace RmlKit::UI {

class VirtualGridElement final : public Rml::Element, private Rml::EventListener {
public:
    explicit VirtualGridElement(const Rml::String& tag);
    ~VirtualGridElement() override;

protected:
    void OnLayout() override;
    void OnAttributeChange(const Rml::ElementAttributes& changed) override;

private:
    void ProcessEvent(Rml::Event& event) override;
    void dispatchViewport();
    void scrollToEnd();
    void scrollToRequest(const Rml::String& request);

    Rml::Vector2f dispatchedClient{-1, -1};
    float dispatchedScroll = -1;
    bool following = false;
    bool atEnd = true;
};

}
