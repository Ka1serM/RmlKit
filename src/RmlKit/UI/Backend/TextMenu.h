#pragma once

#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/ObserverPtr.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class Context;
class Element;
}

namespace RmlKit::UI {

//! Sends an editing shortcut (Ctrl+A/C/V/X/Z/Y) to a text field, which RmlUi handles with Ctrl on every platform.
void sendEditKey(Rml::Element& field, Rml::Input::KeyIdentifier key);

//! The menu of every text field in a window: a right click opens Cut, Copy, Paste and Select all.
class TextMenu final : public Rml::EventListener {
public:
    explicit TextMenu(Rml::Context& context);
    TextMenu(const TextMenu&) = delete;
    TextMenu& operator=(const TextMenu&) = delete;
    ~TextMenu() override;

    void ProcessEvent(Rml::Event& event) override;

private:
    void open(Rml::Element& field, Rml::Vector2f mouse);
    void close();
    void run(const Rml::String& action);

    Rml::Context& context;
    Rml::ObserverPtr<Rml::Element> field;
    Rml::ObserverPtr<Rml::Element> menu;
};

}
