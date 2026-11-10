#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace RmlKit::UI {

struct MessageBoxModel {
    Rml::String level;
    Rml::String icon;
    Rml::String title;
    Rml::String text;
};

void bind(Rml::DataModelConstructor& model, MessageBoxModel& state);

}
