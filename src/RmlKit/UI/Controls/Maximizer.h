#pragma once

#include <optional>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Types.h>

namespace RmlKit::UI {

class Maximizer {
public:
    Maximizer(Rml::Element& element, Rml::EventListener& listener) : element(element), listener(listener) {}
    ~Maximizer();
    Maximizer(const Maximizer&) = delete;
    Maximizer& operator=(const Maximizer&) = delete;

    bool isMaximized() const { return element.IsClassSet("maximized"); }
    void set(bool maximized);
    void toggle() { set(!isMaximized()); }
    void update();
    bool handleKey(Rml::Event& event);

private:
    Rml::Element& element;
    Rml::EventListener& listener;
    std::optional<Rml::Vector2f> offset;
    std::optional<Rml::Vector2f> size;
    Rml::Context* keyContext = nullptr;
};

}
