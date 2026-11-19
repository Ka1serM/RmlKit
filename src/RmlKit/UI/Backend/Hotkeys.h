#pragma once

#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class Context;
class Element;
}

namespace RmlKit::UI {

//! A key and its modifiers, written like "Ctrl+Shift+S", "Delete" or "F2"; "Mod" is Cmd on macOS and Ctrl elsewhere.
struct Hotkey {
    Rml::Input::KeyIdentifier key = Rml::Input::KI_UNKNOWN;
    int modifiers = 0;
    bool operator==(const Hotkey&) const = default;
};

std::optional<Hotkey> parseHotkey(std::string_view text);
//! A field that takes typed text, so it keeps plain keys and the editing shortcuts from the hotkeys.
bool isTextField(const Rml::Element* element);
//! The hotkey as the platform writes it, for a tooltip or a menu hint.
Rml::String formatHotkey(const Hotkey& hotkey);

//! Keyboard shortcuts of a window. In RML, hotkey="Ctrl+S" on a shown element clicks it, so its data-event-click runs, and the tooltip shows the key; <hotkey key="Delete" data-event-click="..."/> does the same without anything to show. In C++, bind runs a function.
class Hotkeys final : public Rml::EventListener {
public:
    explicit Hotkeys(Rml::Context& context);
    Hotkeys(const Hotkeys&) = delete;
    Hotkeys& operator=(const Hotkeys&) = delete;
    ~Hotkeys() override;

    //! A later binding of the same key replaces the earlier one; a key that parseHotkey cannot read is a fatal error.
    void bind(std::string_view hotkey, std::move_only_function<void()> action);
    void unbind(std::string_view hotkey);

    void ProcessEvent(Rml::Event& event) override;

private:
    bool clickElement(const Hotkey& pressed) const;

    Rml::Context& context;
    std::vector<std::pair<Hotkey, std::move_only_function<void()>>> bindings;
};

}
