#pragma once

#include <RmlUi/Core/Element.h>

namespace RmlKit::UI {

//! <splitter min-size max-size default-size>: a drag handle sizing the flex item after it, or before it with target-before="true", in dp.
class SplitterElement final : public Rml::Element {
public:
    explicit SplitterElement(const Rml::String& tag);

protected:
    void OnUpdate() override;
    void ProcessDefaultAction(Rml::Event& event) override;

private:
    Rml::Element* target();
    float pointer(const Rml::Event& event) const;
    float clamped(float wanted);
    void apply();

    float size = -1;
    float appliedSize = -1;
    float dragStartPointer = 0;
    float dragStartSize = 0;
};

}
