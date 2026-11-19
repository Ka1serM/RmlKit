#include "RmlKit/UI/Backend/TextMenu.h"

#include <algorithm>
#include <array>
#include <format>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlTextArea.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/SystemInterface.h>

#include "RmlKit/UI/Backend/Hotkeys.h"

#if PLATFORM_WEB
//! NOLINTNEXTLINE(readability-identifier-naming) The name is an emscripten JS library import from web/library.js.
extern "C" void rmlkit_read_paste();
#else
#include <SDL3/SDL_clipboard.h>
#endif

namespace RmlKit::UI {

namespace {

constexpr const char* actionAttribute = "text-menu-action";
constexpr float marginDp = 4.0f;

struct Selection {
    int start = 0;
    int end = 0;
    Rml::String text;
};

Selection selectionOf(Rml::Element& field) {
    Selection selection;
    if (auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(&field))
        input->GetSelection(&selection.start, &selection.end, &selection.text);
    else if (auto* area = rmlui_dynamic_cast<Rml::ElementFormControlTextArea*>(&field))
        area->GetSelection(&selection.start, &selection.end, &selection.text);
    return selection;
}

void select(Rml::Element& field, const int start, const int end) {
    if (auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(&field))
        input->SetSelectionRange(start, end);
    else if (auto* area = rmlui_dynamic_cast<Rml::ElementFormControlTextArea*>(&field))
        area->SetSelectionRange(start, end);
}

bool editable(const Rml::Element& field) {
    return !field.HasAttribute("readonly") && !field.HasAttribute("disabled");
}

bool hidesText(const Rml::Element& field) {
    return field.GetAttribute<Rml::String>("type", "") == "password";
}

bool clipboardHasText() {
#if PLATFORM_WEB
    //! The page reads the browser clipboard only when asked to paste.
    return true;
#else
    return SDL_HasClipboardText();
#endif
}

Rml::Element* actionOf(Rml::Element* element, const Rml::Element& menu) {
    for (; element && element != &menu; element = element->GetParentNode())
        if (element->HasAttribute(actionAttribute))
            return element;
    return nullptr;
}

bool contains(const Rml::Element& ancestor, const Rml::Element* element) {
    for (; element; element = element->GetParentNode())
        if (element == &ancestor)
            return true;
    return false;
}

}

void sendEditKey(Rml::Element& field, const Rml::Input::KeyIdentifier key) {
    Rml::Dictionary parameters;
    parameters["key_identifier"] = static_cast<int>(key);
    parameters["ctrl_key"] = 1;
    parameters["shift_key"] = 0;
    parameters["alt_key"] = 0;
    parameters["meta_key"] = 0;
    field.DispatchEvent(Rml::EventId::Keydown, parameters);
}

TextMenu::TextMenu(Rml::Context& context) : context(context) {
    //! In the capture phase, so a right click keeps the selection that the field would move its cursor over.
    context.AddEventListener("mousedown", this, true);
    context.AddEventListener("click", this);
    context.AddEventListener("keydown", this, true);
    context.AddEventListener("mousescroll", this, true);
}

TextMenu::~TextMenu() {
    close();
    context.RemoveEventListener("mousedown", this, true);
    context.RemoveEventListener("click", this);
    context.RemoveEventListener("keydown", this, true);
    context.RemoveEventListener("mousescroll", this, true);
}

void TextMenu::ProcessEvent(Rml::Event& event) {
    Rml::Element* target = event.GetTargetElement();
    switch (event.GetId()) {
    case Rml::EventId::Mousedown: {
        if (menu && contains(*menu.get(), target))
            return;
        close();
        if (event.GetParameter<int>("button", 0) != 1 || !isTextField(target) || target->HasAttribute("disabled"))
            return;
        event.StopPropagation();
        open(*target, {event.GetParameter<float>("mouse_x", 0), event.GetParameter<float>("mouse_y", 0)});
        return;
    }
    case Rml::EventId::Click:
        if (!menu || !contains(*menu.get(), target))
            return;
        if (Rml::Element* item = actionOf(target, *menu.get()); item && !item->IsClassSet("disabled")) {
            const Rml::String action = item->GetAttribute<Rml::String>(actionAttribute, "");
            run(action);
        }
        event.StopPropagation();
        return;
    case Rml::EventId::Keydown:
        if (!menu)
            return;
        if (event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN) == Rml::Input::KI_ESCAPE)
            event.StopPropagation();
        close();
        return;
    default:
        close();
        return;
    }
}

void TextMenu::open(Rml::Element& target, const Rml::Vector2f mouse) {
    Rml::ElementDocument* document = target.GetOwnerDocument();
    if (!document)
        return;
    if (context.GetFocusElement() != &target)
        target.Focus(true);
    field = target.GetObserverPtr();

    const Selection selection = selectionOf(target);
    const bool selected = selection.end > selection.start && !hidesText(target);
    const bool writable = editable(target);
    struct Item {
        const char* action;
        const char* label;
        const char* hotkey;
        bool enabled;
    };
    const std::array items{
        Item{"cut", "Cut", "Mod+X", selected && writable},
        Item{"copy", "Copy", "Mod+C", selected},
        Item{"paste", "Paste", "Mod+V", writable && clipboardHasText()},
        Item{"select", "Select all", "Mod+A", true},
    };
    Rml::String markup;
    for (const Item& item : items) {
        markup += std::format(R"(<div class="menu-item{}" {}="{}"><span>{}</span><span class="kbd">{}</span></div>)", item.enabled ? "" : " disabled",
                              actionAttribute, item.action, item.label, formatHotkey(*parseHotkey(item.hotkey)));
        if (item.action == std::string_view("paste"))
            markup += R"(<div class="menu-separator"></div>)";
    }

    Rml::Element* element = document->AppendChild(document->CreateElement("div"));
    element->SetClassNames("menu text-menu");
    element->SetAttribute("selection-start", selection.start);
    element->SetAttribute("selection-end", selection.end);
    element->SetInnerRML(markup);
    document->UpdateDocument();
    menu = element->GetObserverPtr();

    const float dp = context.GetDensityIndependentPixelRatio();
    const float margin = marginDp * dp;
    const Rml::Vector2f viewport(context.GetDimensions());
    const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
    const Rml::Vector2f origin = document->GetAbsoluteOffset(Rml::BoxArea::Border);
    const float left = mouse.x + size.x + margin <= viewport.x ? mouse.x : mouse.x - size.x;
    const float top = mouse.y + size.y + margin <= viewport.y ? mouse.y : mouse.y - size.y;
    element->SetProperty(Rml::PropertyId::Left, Rml::Property(std::max(margin, left) - origin.x, Rml::Unit::PX));
    element->SetProperty(Rml::PropertyId::Top, Rml::Property(std::max(margin, top) - origin.y, Rml::Unit::PX));
}

void TextMenu::close() {
    if (Rml::Element* element = menu.get())
        element->GetParentNode()->RemoveChild(element);
    menu.reset();
}

void TextMenu::run(const Rml::String& action) {
    Rml::Element* target = field.get();
    if (!target || !menu) {
        close();
        return;
    }
    //! The click focused the menu, and the field lost its selection with the focus.
    const int start = menu->GetAttribute<int>("selection-start", 0);
    const int end = menu->GetAttribute<int>("selection-end", 0);
    close();
    target->Focus(true);
    select(*target, start, end);
    if (action == "cut")
        sendEditKey(*target, Rml::Input::KI_X);
    else if (action == "copy")
        sendEditKey(*target, Rml::Input::KI_C);
    else if (action == "select")
        sendEditKey(*target, Rml::Input::KI_A);
    else if (action == "paste")
#if PLATFORM_WEB
        rmlkit_read_paste();
#else
        sendEditKey(*target, Rml::Input::KI_V);
#endif
}

}
