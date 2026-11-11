#include "RmlKit/UI/Controllers/LogsController.h"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/SystemInterface.h>

#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Models/Model.h"

namespace RmlKit::UI {

namespace {

constexpr std::array<const char*, 5> levels = {"", "INFO", "DONE", "WARN", "ERR"};

}

LogsController::LogsController(Model& model) : model(model) {
    model.bindEvent("logsLevel", &LogsController::onLevel, this);
    model.bindEvent("logsSearch", &LogsController::onSearch, this);
    model.bindEvent("logsSource", &LogsController::onSource, this);
    model.bindEvent("logsViewport", &LogsController::onViewport, this);
    model.bindEvent("logsPick", &LogsController::onPick, this);
    model.bindEvent("logsCopy", &LogsController::onCopy, this);
    model.bindEvent("logsCopyPicked", &LogsController::onCopyPicked, this);
}

void LogsController::onLevel(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
    if (arguments.size() != 1)
        fatal("logsLevel takes one argument: the level");
    Rml::String level = arguments[0].Get<Rml::String>();
    if (!std::ranges::contains(levels, level))
        fatal(std::format("logsLevel: {} is not a level", level));
    model.logs.levelFilter = std::move(level);
    filter();
}

void LogsController::onSearch(Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
    Model::requireNoArguments(arguments, "logsSearch");
    model.logs.search = event.GetParameter<Rml::String>("value", "");
    filter();
}

void LogsController::onSource(Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
    Model::requireNoArguments(arguments, "logsSource");
    model.logs.sourceFilter = event.GetParameter<Rml::String>("value", "");
    filter();
}

void LogsController::onViewport(Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
    Model::requireNoArguments(arguments, "logsViewport");
    const bool changed = model.logs.setViewport(event.GetParameter("scroll_top", 0.0f), event.GetParameter("height", 0.0f), event.GetParameter("at_end", true));
    if (changed)
        model.dirty("logRows");
}

void LogsController::onPick(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
    const int row = static_cast<int>(Model::indexArgument(arguments, "logsPick", model.logs.lines.size()));
    model.logs.pickedRow = row;
    model.logs.picked = model.logs.lines[static_cast<std::size_t>(row)];
    model.dirty("logs");
}

void LogsController::onCopy(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
    copy(static_cast<int>(Model::indexArgument(arguments, "logsCopy", model.logs.lines.size())));
}

void LogsController::onCopyPicked(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
    Model::requireNoArguments(arguments, "logsCopyPicked");
    if (model.logs.pickedRow < 0)
        fatal("logsCopyPicked: no line is picked");
    copy(model.logs.pickedRow);
}

void LogsController::filter() {
    model.logs.filter();
    model.dirty("logs");
    model.dirty("logRows");
}

void LogsController::copy(const int row) {
    model.logs.copiedRow = row;
    const LogLine& line = model.logs.lines[static_cast<std::size_t>(row)];
    Rml::GetSystemInterface()->SetClipboardText(std::format("{} {} {} · {}", line.time, line.level, line.source, line.message));
    model.dirty("logs");
}

}
