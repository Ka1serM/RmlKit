#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Types.h>

namespace RmlKit::UI {

class Model;

class LogsController {
public:
    explicit LogsController(Model& model);
    LogsController(const LogsController&) = delete;
    LogsController& operator=(const LogsController&) = delete;

private:
    void onLevel(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);
    void onSearch(Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments);
    void onSource(Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments);
    void onViewport(Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments);
    void onPick(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);
    void onCopy(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);
    void onCopyPicked(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);

    void filter();
    void copy(int row);
    int rowArgument(const Rml::VariantList& arguments, const char* event) const;

    Model& model;
};

}
