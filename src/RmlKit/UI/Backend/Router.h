#pragma once

#include <unordered_map>
#include <unordered_set>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Types.h>

#include "RmlKit/UI/Backend/ViewUrl.h"

namespace Rml {
class Context;
class Element;
class ElementDocument;
}

namespace RmlKit::UI {

class Model;

class Router final : public Rml::EventListener {
public:
    Router(Rml::Context& context, Model& model);
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;
    ~Router() override;

    void ProcessEvent(Rml::Event& event) override;

    void open();
    void show(const Rml::String& name);
    void showFromUrl(const Rml::String& name);
    void showUrlView();
    void reload();

private:
    void onShowView(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);

    Rml::Element& loadView(const Rml::String& name);
    void close();

    Rml::Context& context;
    Model& model;
    Rml::ElementDocument* document = nullptr;
    std::unordered_map<Rml::String, Rml::Element*> views;
    Rml::Element* shown = nullptr;
    std::unordered_set<Rml::String> visited;
    ViewUrl url;
};

}
