#pragma once

#include <format>
#include <utility>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Models/LogsModel.h"
#include "RmlKit/UI/Models/MessageBoxModel.h"
#include "RmlKit/UI/Models/NavigationModel.h"

namespace Rml {
class Context;
}

namespace RmlKit::UI {

enum class ReportLevel { Info, Done, Warning, Error };

class Model {
public:
    explicit Model(Rml::Context& context);
    ~Model();
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    //! RmlUi cannot unbind an event, so the owner must outlive the model.
    template <typename Owner>
    void bindEvent(const Rml::String& name, Rml::DataModelConstructor::DataEventMemberFunc<Owner> callback, Owner* owner) {
        if (!constructor.BindEventCallback(name, callback, owner))
            fatal(std::format("RmlUi cannot bind the data event {}", name));
    }
    void dirty(const Rml::String& variable) {
        handle.DirtyVariable(variable);
        changed = true;
    }
    bool takeChanged() { return std::exchange(changed, false); }
    void report(ReportLevel level, const Rml::String& source, Rml::String message);

    NavigationModel navigation;
    LogsModel logs;
    MessageBoxModel messageBox;
    Rml::String productName;
    bool windowButtons = !PLATFORM_WEB;

    Rml::DataModelConstructor& getConstructor() { return constructor; }

private:
    Rml::Context& context;
    Rml::DataModelConstructor constructor;
    Rml::DataModelHandle handle;
    bool changed = false;
};

}
