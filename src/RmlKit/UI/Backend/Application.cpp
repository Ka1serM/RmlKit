#include "RmlKit/UI/Backend/Application.h"

#include <cstdint>
#include <format>
#include <limits>
#include <unordered_set>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

Application::Sdl::Sdl() {
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition");
    setPlatformHints();
    //! On Wayland, the app id must match the name of the launcher.
    if (!SDL_SetAppMetadata(PRODUCT_NAME, PRODUCT_VERSION, APP_ID))
        fatal(std::format("SDL_SetAppMetadata failed: {}", SDL_GetError()));
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        fatal(std::format("SDL_Init failed: {}", SDL_GetError()));
}

Application::Sdl::~Sdl() {
    SDL_Quit();
}

Application::Application()
    : window(PRODUCT_NAME, 1600, 1056), rml(window), context(createContext(window.pixelSize())), model(*context),
      logRoute(rml.getSystem(), model), chrome(*context, model, window.getNative(), [this] { running = false; }),
      linkDrag(*context, model, window.getNative()), fileDrop(*context, window.getNative()), router(*context, model), tooltip(*context),
      messageBoxes(*context, model)
#ifndef NDEBUG
      ,
      devTools(*context, router)
#endif
{
    context->SetDensityIndependentPixelRatio(SDL_GetWindowDisplayScale(window.getNative()));
    wakeEvent = SDL_RegisterEvents(1);
    if (wakeEvent == 0)
        fatal(std::format("SDL_RegisterEvents failed: {}", SDL_GetError()));
    watchResize();
}

std::unique_ptr<Rml::Context, Application::ContextRemover> Application::createContext(const Rml::Vector2i size) {
    std::unique_ptr<Rml::Context, ContextRemover> context(Rml::CreateContext("main", size));
    if (!context)
        fatal("RmlUi cannot create the main context");
    return context;
}

Application::~Application() {
    if (wakeTimer)
        SDL_RemoveTimer(wakeTimer);
    unwatchResize();
}

double Application::nextFrameDelay() const {
    if (!window.drawable())
        return std::numeric_limits<double>::infinity();
    return context->GetNextUpdateDelay() - static_cast<double>(SDL_GetTicksNS() - lastUpdateNs) / SDL_NS_PER_SECOND;
}

void Application::closeDocuments() {
    context->UnloadAllDocuments();
    //! RmlUi releases unloaded documents, and with them their elements, in the next update.
    context->Update();
}

void Application::handle(const SDL_Event& event) {
    framePending = true;
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        if (pendingMotion
            && (pendingMotion->motion.windowID != event.motion.windowID || pendingMotion->motion.which != event.motion.which
                || pendingMotion->motion.state != event.motion.state))
            flushMotion();
        if (pendingMotion) {
            SDL_Event merged = event;
            merged.motion.xrel += pendingMotion->motion.xrel;
            merged.motion.yrel += pendingMotion->motion.yrel;
            pendingMotion = merged;
        } else {
            pendingMotion = event;
        }
        return;
    }
    flushMotion();
    SDL_Event copy = event;
    dispatch(copy);
}

void Application::flushMotion() {
    if (!pendingMotion)
        return;
    SDL_Event motion = *pendingMotion;
    pendingMotion.reset();
    dispatch(motion);
}

bool Application::iterate() {
    flushMotion();
    if (!running)
        return false;
    constexpr double dueWithinSeconds = 0.001;
    if (framePending || nextFrameDelay() <= dueWithinSeconds) {
        framePending = false;
        draw();
    }
    scheduleNextFrame();
    return running;
}

void Application::dispatch(SDL_Event& event) {
    if (event.type == wakeEvent)
        return;
    if (dispatcher.dispatch(event))
        return;
    chrome.handle(event);
    linkDrag.handle(event);
    fileDrop.handle(event);
#ifndef NDEBUG
    if (devTools.handle(event))
        return;
#endif
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        running = false;
        return;
    case SDL_EVENT_TEXT_EDITING:
        rml.getTextInput().HandleEdit(event.edit);
        return;
    default:
        RmlSDL::InputEventHandler(context.get(), window.getNative(), event);
    }
}

void Application::draw() {
    if (!window.drawable())
        return;
    window.prepare(*context);
    update();
    window.render(*context);
}

void Application::update() {
    //! An RmlUi layout can change the data model, so the extra updates lay out that change before the frame draws.
    constexpr int maxExtraUpdates = 2;
    model.takeChanged();
    lastUpdateNs = SDL_GetTicksNS();
    refreshArrivedTextures();
    context->Update();
    for (int extra = 0; extra < maxExtraUpdates && model.takeChanged(); ++extra)
        context->Update();
}

void Application::refreshArrivedTextures() {
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
