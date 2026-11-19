#include "RmlKit/UI/Backend/Hotkeys.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <ranges>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

namespace {

using Rml::Input::KeyIdentifier;
namespace Input = Rml::Input;

struct NamedKey {
    std::string_view name;
    KeyIdentifier key;
};

constexpr std::array namedKeys{
    NamedKey{"Space", Input::KI_SPACE},       NamedKey{"Enter", Input::KI_RETURN},      NamedKey{"Return", Input::KI_RETURN},
    NamedKey{"Escape", Input::KI_ESCAPE},     NamedKey{"Esc", Input::KI_ESCAPE},        NamedKey{"Tab", Input::KI_TAB},
    NamedKey{"Backspace", Input::KI_BACK},    NamedKey{"Delete", Input::KI_DELETE},     NamedKey{"Del", Input::KI_DELETE},
    NamedKey{"Insert", Input::KI_INSERT},     NamedKey{"Home", Input::KI_HOME},         NamedKey{"End", Input::KI_END},
    NamedKey{"PageUp", Input::KI_PRIOR},      NamedKey{"PageDown", Input::KI_NEXT},     NamedKey{"Left", Input::KI_LEFT},
    NamedKey{"Right", Input::KI_RIGHT},       NamedKey{"Up", Input::KI_UP},             NamedKey{"Down", Input::KI_DOWN},
    NamedKey{"Plus", Input::KI_OEM_PLUS},     NamedKey{"Minus", Input::KI_OEM_MINUS},   NamedKey{"Comma", Input::KI_OEM_COMMA},
    NamedKey{"Period", Input::KI_OEM_PERIOD},
};

#if defined(__APPLE__)
constexpr int modKey = Input::KM_META;
#else
constexpr int modKey = Input::KM_CTRL;
#endif

bool equalNoCase(const std::string_view a, const std::string_view b) {
    return std::ranges::equal(a, b, [](const unsigned char x, const unsigned char y) { return std::tolower(x) == std::tolower(y); });
}

std::optional<KeyIdentifier> keyOf(const std::string_view name) {
    if (name.size() == 1 && std::isalpha(static_cast<unsigned char>(name[0])))
        return static_cast<KeyIdentifier>(Input::KI_A + (std::toupper(static_cast<unsigned char>(name[0])) - 'A'));
    if (name.size() == 1 && std::isdigit(static_cast<unsigned char>(name[0])))
        return static_cast<KeyIdentifier>(Input::KI_0 + (name[0] - '0'));
    if (name.size() >= 2 && (name[0] == 'F' || name[0] == 'f') && std::ranges::all_of(name.substr(1), [](const unsigned char c) { return std::isdigit(c); })) {
        const int number = std::stoi(std::string(name.substr(1)));
        if (number >= 1 && number <= 24)
            return static_cast<KeyIdentifier>(Input::KI_F1 + number - 1);
    }
    for (const NamedKey& named : namedKeys)
        if (equalNoCase(named.name, name))
            return named.key;
    return std::nullopt;
}

std::optional<int> modifierOf(const std::string_view name) {
    if (equalNoCase(name, "Ctrl") || equalNoCase(name, "Control"))
        return Input::KM_CTRL;
    if (equalNoCase(name, "Shift"))
        return Input::KM_SHIFT;
    if (equalNoCase(name, "Alt") || equalNoCase(name, "Option"))
        return Input::KM_ALT;
    if (equalNoCase(name, "Meta") || equalNoCase(name, "Cmd") || equalNoCase(name, "Super"))
        return Input::KM_META;
    if (equalNoCase(name, "Mod"))
        return modKey;
    return std::nullopt;
}

Hotkey pressedOf(const Rml::Event& event) {
    int modifiers = 0;
    if (event.GetParameter<int>("ctrl_key", 0))
        modifiers |= Input::KM_CTRL;
    if (event.GetParameter<int>("shift_key", 0))
        modifiers |= Input::KM_SHIFT;
    if (event.GetParameter<int>("alt_key", 0))
        modifiers |= Input::KM_ALT;
    if (event.GetParameter<int>("meta_key", 0))
        modifiers |= Input::KM_META;
    return {static_cast<KeyIdentifier>(event.GetParameter<int>("key_identifier", Input::KI_UNKNOWN)), modifiers};
}

//! A text field keeps its own keys: plain typing and the editing shortcuts.
bool textFieldKeeps(const Hotkey& pressed) {
    constexpr int command = Input::KM_CTRL | Input::KM_ALT | Input::KM_META;
    if (!(pressed.modifiers & command))
        return true;
    constexpr std::array editing{Input::KI_A, Input::KI_C, Input::KI_V, Input::KI_X, Input::KI_Z, Input::KI_Y, Input::KI_LEFT, Input::KI_RIGHT, Input::KI_BACK, Input::KI_DELETE};
    return (pressed.modifiers & ~Input::KM_SHIFT) == modKey && std::ranges::contains(editing, pressed.key);
}

}

bool isTextField(const Rml::Element* element) {
    if (!element)
        return false;
    if (element->GetTagName() == "textarea")
        return true;
    if (element->GetTagName() != "input")
        return false;
    const Rml::String type = element->GetAttribute<Rml::String>("type", "text");
    return type == "text" || type == "password" || type == "search" || type == "number";
}

std::optional<Hotkey> parseHotkey(const std::string_view text) {
    Hotkey hotkey;
    for (const auto part : std::views::split(text, '+')) {
        const std::string_view name(part.begin(), part.end());
        if (hotkey.key != Input::KI_UNKNOWN)
            return std::nullopt;
        if (const std::optional<int> modifier = modifierOf(name))
            hotkey.modifiers |= *modifier;
        else if (const std::optional<KeyIdentifier> key = keyOf(name))
            hotkey.key = *key;
        else
            return std::nullopt;
    }
    if (hotkey.key == Input::KI_UNKNOWN)
        return std::nullopt;
    return hotkey;
}

Rml::String formatHotkey(const Hotkey& hotkey) {
    Rml::String text;
#if defined(__APPLE__)
    constexpr std::array modifiers{std::pair{Input::KM_CTRL, "Ctrl+"}, std::pair{Input::KM_ALT, "Option+"}, std::pair{Input::KM_SHIFT, "Shift+"}, std::pair{Input::KM_META, "Cmd+"}};
#else
    constexpr std::array modifiers{std::pair{Input::KM_CTRL, "Ctrl+"}, std::pair{Input::KM_ALT, "Alt+"}, std::pair{Input::KM_SHIFT, "Shift+"}, std::pair{Input::KM_META, "Super+"}};
#endif
    for (const auto& [bit, name] : modifiers)
        if (hotkey.modifiers & bit)
            text += name;
    if (hotkey.key >= Input::KI_A && hotkey.key <= Input::KI_Z)
        return text + static_cast<char>('A' + (hotkey.key - Input::KI_A));
    if (hotkey.key >= Input::KI_0 && hotkey.key <= Input::KI_9)
        return text + static_cast<char>('0' + (hotkey.key - Input::KI_0));
    if (hotkey.key >= Input::KI_F1 && hotkey.key <= Input::KI_F24)
        return text + std::format("F{}", hotkey.key - Input::KI_F1 + 1);
    for (const NamedKey& named : namedKeys)
        if (named.key == hotkey.key)
            return text + Rml::String(named.name);
    return text;
}

Hotkeys::Hotkeys(Rml::Context& context) : context(context) {
    context.AddEventListener("keydown", this);
}

Hotkeys::~Hotkeys() {
    context.RemoveEventListener("keydown", this);
}

void Hotkeys::bind(const std::string_view hotkey, std::move_only_function<void()> action) {
    const std::optional<Hotkey> parsed = parseHotkey(hotkey);
    if (!parsed)
        fatal(std::format("RmlKit cannot read the hotkey {}", hotkey));
    unbind(hotkey);
    bindings.emplace_back(*parsed, std::move(action));
}

void Hotkeys::unbind(const std::string_view hotkey) {
    if (const std::optional<Hotkey> parsed = parseHotkey(hotkey))
        std::erase_if(bindings, [&](const auto& binding) { return binding.first == *parsed; });
}

void Hotkeys::ProcessEvent(Rml::Event& event) {
    const Hotkey pressed = pressedOf(event);
    if (pressed.key == Input::KI_UNKNOWN)
        return;
    if (isTextField(event.GetTargetElement()) && textFieldKeeps(pressed))
        return;
    if (clickElement(pressed)) {
        event.StopPropagation();
        return;
    }
    for (auto& [hotkey, action] : bindings)
        if (hotkey == pressed) {
            action();
            event.StopPropagation();
            return;
        }
}

//! The top document wins, so an open dialog takes a key before the window under it.
bool Hotkeys::clickElement(const Hotkey& pressed) const {
    for (int index = context.GetNumDocuments() - 1; index >= 0; --index) {
        Rml::ElementDocument* document = context.GetDocument(index);
        if (!document->IsVisible())
            continue;
        Rml::ElementList elements;
        document->QuerySelectorAll(elements, "[hotkey], hotkey");
        for (Rml::Element* element : elements) {
            const bool hidden = element->GetTagName() == "hotkey";
            const Rml::String text = element->GetAttribute<Rml::String>(hidden ? "key" : "hotkey", "");
            if (parseHotkey(text) != pressed || element->HasAttribute("disabled"))
                continue;
            if (!hidden && !element->IsVisible(true))
                continue;
            element->Click();
            return true;
        }
    }
    return false;
}

}
