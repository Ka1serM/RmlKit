#include "RmlKit/UI/Models/Model.h"

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
}

Model::~Model() {
    context.RemoveDataModel(modelName);
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
