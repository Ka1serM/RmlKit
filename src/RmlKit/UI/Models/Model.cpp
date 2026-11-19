#include "RmlKit/UI/Models/Model.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include <RmlUi/Core/Context.h>
#include <SDL3/SDL_log.h>

#if PLATFORM_WEB
#include <emscripten/console.h>
#endif

#include "RmlKit/Core/LocalTime.h"
#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Backend/WindowButtonLayout.h"

namespace RmlKit::UI {

namespace {
constexpr const char* modelName = "app";
}

Model::Model(Rml::Context& context) : productName(PRODUCT_NAME), context(context), constructor(context.CreateDataModel(modelName)) {
    if (!constructor)
        fatal("RmlUi cannot create the app data model");
    handle = constructor.GetModelHandle();

    constructor.RegisterArray<std::vector<Rml::String>>();
    auto line = constructor.RegisterStruct<LogLine>();
    line.RegisterMember("time", &LogLine::time);
    line.RegisterMember("level", &LogLine::level);
    line.RegisterMember("source", &LogLine::source);
    line.RegisterMember("message", &LogLine::message);
    line.RegisterMember("index", &LogLine::index);
    constructor.RegisterArray<std::vector<LogLine>>();
    bind(constructor, navigation);
    bind(constructor, logs);
    bind(constructor, messageBox);
    constructor.Bind("productName", &productName);
    constructor.Bind("windowButtons", &windowButtons);
    constructor.Bind("windowMaximized", &windowMaximized);
    constructor.Bind("windowFocused", &windowFocused);
    const WindowButtonLayout& layout = getWindowButtonLayout();
    windowButtonStyle = layout.style;
    windowButtonsStart = layout.start;
    windowButtonsEnd = layout.end;
    constructor.Bind("windowButtonStyle", &windowButtonStyle);
    constructor.Bind("windowButtonsStart", &windowButtonsStart);
    constructor.Bind("windowButtonsEnd", &windowButtonsEnd);
}

Model::~Model() {
    if (source)
        std::erase(source->windows, this);
    for (Model* window : windows)
        window->source = nullptr;
    context.RemoveDataModel(modelName);
}

void Model::bindData(Binder binder) {
    binder(constructor);
    for (Model* window : windows)
        binder(window->constructor);
    binders.push_back(std::move(binder));
}

void Model::share(Model& window) {
    if (window.source || !window.binders.empty() || !window.windows.empty())
        fatal("A window model can share only one model and must have no shared bindings of its own.");
    for (Binder& binder : binders)
        binder(window.constructor);
    window.source = this;
    windows.push_back(&window);
}

void Model::dirty(const Rml::String& variable) {
    Model& root = source ? *source : *this;
    root.handle.DirtyVariable(variable);
    root.changed = true;
    for (Model* window : root.windows) {
        window->handle.DirtyVariable(variable);
        window->changed = true;
    }
}

void Model::refreshShared() {
    Model& root = source ? *source : *this;
    std::vector<Model*> group = root.windows;
    group.push_back(&root);
    for (Model* model : group) {
        if (model == this)
            continue;
        model->handle.DirtyAllVariables();
        model->changed = true;
    }
}

void Model::report(const ReportLevel level, const Rml::String& source, Rml::String message) {
    LogLine line;
    switch (level) {
    case ReportLevel::Info:
        line.level = "INFO";
        break;
    case ReportLevel::Done:
        line.level = "DONE";
        break;
    case ReportLevel::Warning:
        line.level = "WARN";
        break;
    case ReportLevel::Error:
        line.level = "ERR";
        break;
    }
#if PLATFORM_WEB
    //! SDL writes every line to stderr, which the browser shows as an error.
    const std::string text = std::format("{}: {}", source, message);
    if (level == ReportLevel::Error)
        emscripten_console_error(text.c_str());
    else if (level == ReportLevel::Warning)
        emscripten_console_warn(text.c_str());
    else
        emscripten_console_log(text.c_str());
#else
    const SDL_LogPriority priority = level == ReportLevel::Error     ? SDL_LOG_PRIORITY_ERROR
                                     : level == ReportLevel::Warning ? SDL_LOG_PRIORITY_WARN
                                                                     : SDL_LOG_PRIORITY_INFO;
    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "%s: %s", source.c_str(), message.c_str());
#endif
    line.time = Core::localTime(std::chrono::system_clock::now(), "%H:%M:%S");
    line.source = source;
    line.message = std::move(message);
    logs.add(std::move(line));
    dirty("logs");
    dirty("logLines");
    dirty("logRows");
}

void Model::requireNoArguments(const Rml::VariantList& arguments, const char* event) {
    if (!arguments.empty())
        fatal(std::format("{} takes no argument", event));
}

std::size_t Model::indexOf(const Rml::Variant& argument, const char* event) {
    const int index = argument.Get<int>(-1);
    if (index < 0)
        fatal(std::format("{}: the index is not a number from 0", event));
    return static_cast<std::size_t>(index);
}

std::size_t Model::indexArgument(const Rml::VariantList& arguments, const char* event) {
    if (arguments.size() != 1)
        fatal(std::format("{} takes one argument: the index", event));
    return indexOf(arguments[0], event);
}

std::size_t Model::indexArgument(const Rml::VariantList& arguments, const char* event, const std::size_t count) {
    const std::size_t index = indexArgument(arguments, event);
    if (index >= count)
        fatal(std::format("{}: the list has {} rows, not a row {}", event, count, index));
    return index;
}

}
