#pragma once

#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class Context;
class Element;
}

namespace RmlKit::UI {

class Tooltip final : public Rml::EventListener {
public:
    explicit Tooltip(Rml::Context& context);
    Tooltip(const Tooltip&) = delete;
    Tooltip& operator=(const Tooltip&) = delete;
    ~Tooltip() override;

    void ProcessEvent(Rml::Event& event) override;

private:
    static Rml::Element* hostOf(Rml::Element* element);
    static Rml::String markupOf(const Rml::Element& host);
    Rml::Element* bubble() const;
    void show(const Rml::Element& host, Rml::Vector2f cursor);
    void hide();

    Rml::Context& context;
    Rml::String shownMarkup;
};

}
