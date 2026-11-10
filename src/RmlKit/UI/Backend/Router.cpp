#include "RmlKit/UI/Backend/Router.h"

#include <format>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/ID.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Property.h>
#include <RmlUi/Core/StyleTypes.h>

#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Models/Model.h"

namespace RmlKit::UI {

namespace {

constexpr std::string_view crumbSeparator = " / ";

Rml::String viewPath(const Rml::String& name) {
    return std::format("UI/Views/{}.rml", name);
}

bool hasView(const Rml::String& name) {
    Rml::FileInterface& files = *Rml::GetFileInterface();
    const Rml::FileHandle file = name.empty() ? 0 : files.Open(viewPath(name));
    if (!file)
        return false;
    files.Close(file);
    return true;
}

void setHidden(Rml::Element& view, const bool hidden) {
    if (hidden)
        view.SetProperty(Rml::PropertyId::Display, Rml::Property(Rml::Style::Display::None));
    else
        view.RemoveProperty(Rml::PropertyId::Display);
}

}

Router::Router(Rml::Context& context, Model& model) : context(context), model(model), url(*this) {
    model.bindEvent("showView", &Router::onShowView, this);
    context.AddEventListener("keydown", this);
}

Router::~Router() {
    context.RemoveEventListener("keydown", this);
}

void Router::ProcessEvent(Rml::Event& event) {
    if (event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN) != Rml::Input::KI_F)
        return;
    if (!event.GetParameter<int>("ctrl_key", 0) || !shown)
        return;
    auto* field = dynamic_cast<Rml::ElementFormControlInput*>(shown->QuerySelector(".input.search input"));
    if (!field)
        return;
    field->Focus(true);
    field->Select();
    event.StopPropagation();
}

Rml::Element& Router::loadView(const Rml::String& name) {
    Rml::Element* parent = document ? document->GetElementById("views") : nullptr;
    if (!parent)
        fatal(std::format("The view {} needs the main document open", name));
    const Rml::String path = viewPath(name);
    Rml::String rml;
    if (!Rml::GetFileInterface()->LoadFile(path, rml))
        fatal(std::format("RmlUi cannot read {}", path));
    const int index = parent->GetNumChildren();
    Rml::Factory::InstanceElementText(parent, rml);
    Rml::Element* view = parent->GetChild(index);
    if (!view || view->GetId() != name || parent->GetNumChildren() != index + 1)
        fatal(std::format("{} is not the view {}: it needs one element with id=\"{}\"", path, name, name));
    setHidden(*view, true);
    return *view;
}

void Router::open() {
    if (document)
        fatal("The router already opened the main document");
    constexpr std::string_view path = "UI/Documents/main.rml";
    Rml::ElementDocument* loaded = context.LoadDocument(Rml::String(path));
    if (!loaded)
        fatal(std::format("RmlUi cannot load {}", path));
    if (loaded->GetId() != "main") {
        loaded->Close();
        fatal(std::format("{} is not the main document: its <body> needs id=\"main\"", path));
    }
    document = loaded;
    document->Show();
}

void Router::show(const Rml::String& name) {
    auto loaded = views.find(name);
    if (loaded == views.end())
        loaded = views.emplace(name, &loadView(name)).first;
    Rml::Element& view = *loaded->second;
    if (shown == &view)
        return;
    if (shown) {
        setHidden(*shown, true);
        shown->DispatchEvent(Rml::EventId::Hide, {});
    }
    setHidden(view, false);
    document->SetAttribute("nav", view.GetAttribute<Rml::String>("nav", ""));
    shown = &view;
    visited.insert(name);
    url.show(name);

    const Rml::String crumb = view.GetAttribute<Rml::String>("crumb", "");
    const auto separator = crumb.find(crumbSeparator);
    NavigationModel& navigation = model.navigation;
    navigation.view = name;
    navigation.crumbParent = separator == Rml::String::npos ? "" : crumb.substr(0, separator);
    navigation.crumb = separator == Rml::String::npos ? crumb : crumb.substr(separator + crumbSeparator.size());
    navigation.crumbDetail.clear();
    model.dirty("navigation");

    view.Focus();
    view.DispatchEvent(Rml::EventId::Show, {});
}

void Router::showFromUrl(const Rml::String& name) {
    if (visited.contains(name))
        return show(name);
    if (!hasView(name))
        return show("home");
    auto loaded = views.find(name);
    if (loaded == views.end())
        loaded = views.emplace(name, &loadView(name)).first;
    const Rml::String section = loaded->second->GetAttribute<Rml::String>("nav", "");
    show(section == name || hasView(section) ? section : "home");
}

void Router::showUrlView() {
    const Rml::String view = url.getView();
    if (view.empty())
        return show("home");
    showFromUrl(view);
}

void Router::close() {
    if (shown)
        shown->DispatchEvent(Rml::EventId::Hide, {});
    if (document)
        document->Close();
    document = nullptr;
    views.clear();
    shown = nullptr;
}

void Router::reload() {
    if (!document)
        return;
    const Rml::String view = shown ? shown->GetId() : Rml::String();
    close();
    open();
    if (!view.empty())
        show(view);
}

void Router::onShowView(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
    if (arguments.size() != 1)
        fatal("showView takes one argument: the view name");
    show(arguments[0].Get<Rml::String>());
}

}
