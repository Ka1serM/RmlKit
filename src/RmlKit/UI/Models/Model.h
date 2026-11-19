#pragma once

#include <cstddef>
#include <format>
#include <functional>
#include <vector>
#include <utility>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/Variant.h>

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

    using Binder = std::function<void(Rml::DataModelConstructor&)>;

    void bindData(Binder binder);
    //! RmlUi cannot unbind an event, so the owner must outlive the model.
    template <typename Owner>
    void bindEvent(const Rml::String& name, Rml::DataModelConstructor::DataEventMemberFunc<Owner> callback, Owner* owner) {
        bindData([name, callback, owner](Rml::DataModelConstructor& constructor) {
            if (!constructor.BindEventCallback(name, callback, owner))
                fatal(std::format("RmlUi cannot bind the data event {}", name));
        });
    }
    //! An event of one window stays in that window's model, out of the bindings a shared model passes on.
    template <typename Owner>
    void bindWindowEvent(const Rml::String& name, Rml::DataModelConstructor::DataEventMemberFunc<Owner> callback, Owner* owner) {
        if (!constructor.BindEventCallback(name, callback, owner))
            fatal(std::format("RmlUi cannot bind the data event {}", name));
    }
    void share(Model& window);
    //! A data view in one window writes its value straight into the variable, so the other windows of the group look at every variable again.
    void refreshShared();
    void dirty(const Rml::String& variable);
    bool takeChanged() { return std::exchange(changed, false); }
    void report(ReportLevel level, const Rml::String& source, Rml::String message);

    static void requireNoArguments(const Rml::VariantList& arguments, const char* event);
    static std::size_t indexOf(const Rml::Variant& argument, const char* event);
    static std::size_t indexArgument(const Rml::VariantList& arguments, const char* event);
    //! The one argument must be an index below count.
    static std::size_t indexArgument(const Rml::VariantList& arguments, const char* event, std::size_t count);

    NavigationModel navigation;
    LogsModel logs;
    MessageBoxModel messageBox;
    Rml::String productName;
    bool windowButtons = !PLATFORM_WEB;
    bool windowMaximized = false;
    //! False while another window has the keyboard; the title bar buttons then get the class unfocused, as GTK shows its backdrop state.
    bool windowFocused = true;
    //! The title bar buttons of the platform, from WindowButtonLayout.
    Rml::String windowButtonStyle;
    std::vector<Rml::String> windowButtonsStart;
    std::vector<Rml::String> windowButtonsEnd;


private:
    Rml::Context& context;
    Rml::DataModelConstructor constructor;
    Rml::DataModelHandle handle;
    bool changed = false;
    std::vector<Binder> binders;
    std::vector<Model*> windows;
    Model* source = nullptr;
};

}
