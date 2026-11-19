#include "RmlKit/UI/Backend/WindowView.h"

#include <limits>
#include <unordered_set>
#include <utility>

#include <RmlUi/Core/ElementDocument.h>
#include <SDL3/SDL_timer.h>

namespace RmlKit::UI {

namespace {

bool changesValues(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return true;
    case SDL_EVENT_MOUSE_MOTION:
        return event.motion.state != 0;
    default:
        return false;
    }
}

}

WindowView::WindowView(Window& window, const Rml::String& name, std::move_only_function<void()> close)
    : window(window), context(window.openContext(name, textInput)), model(*context), chrome(*context, model, window.getNative(), [this] { this->close(); }),
      tooltip(*context), hotkeys(*context), textMenu(*context), close(std::move(close)) {}

double WindowView::nextFrameDelay() const {
    if (!window.drawable())
        return std::numeric_limits<double>::infinity();
    return context->GetNextUpdateDelay() - static_cast<double>(SDL_GetTicksNS() - lastUpdateNs) / SDL_NS_PER_SECOND;
}

void WindowView::handle(SDL_Event& event) {
    chrome.handle(event);
    switch (event.type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        return close();
    case SDL_EVENT_TEXT_EDITING:
        return textInput.HandleEdit(event.edit);
    default:
        RmlSDL::InputEventHandler(context.get(), window.getNative(), event);
    }
    if (changesValues(event))
        model.refreshShared();
}

bool WindowView::prepare() {
    if (!window.drawable())
        return false;
    context->SetDimensions(overlaySize.value_or(window.pixelSize()));
    update();
    return true;
}

void WindowView::draw() {
    if (!prepare())
        return;
    const Window::Layer layer{*context, {}};
    window.render({&layer, 1});
}

void WindowView::update() {
    //! An RmlUi layout can change the data model, so the extra updates lay out that change before the frame draws.
    constexpr int maxExtraUpdates = 2;
    model.takeChanged();
    lastUpdateNs = SDL_GetTicksNS();
    refreshArrivedTextures();
    styleDocuments();
    context->Update();
    for (int extra = 0; extra < maxExtraUpdates && model.takeChanged(); ++extra)
        context->Update();
}

void WindowView::styleDocuments() {
    const bool light = SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT;
    const bool maximized = !overlaySize && window.maximized();
    for (int i = 0; i < context->GetNumDocuments(); ++i) {
        Rml::ElementDocument* document = context->GetDocument(i);
        document->SetClass("light", light);
        document->SetClass("window-maximized", maximized);
    }
}

void WindowView::refreshArrivedTextures() {
    const std::unordered_set<Rml::String> arrived = window.getRenderer().takeArrivedSources();
    if (arrived.empty())
        return;
    Rml::ElementList images;
    for (int i = 0; i < context->GetNumDocuments(); ++i)
        context->GetDocument(i)->GetElementsByTagName(images, "img");
    //! RmlUi does not lay out an image again when its texture arrives; setting the same src does.
    for (Rml::Element* const image : images) {
        const Rml::String source = image->GetAttribute<Rml::String>("src", "");
        if (arrived.contains(source))
            image->SetAttribute("src", source);
    }
}

}
