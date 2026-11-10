#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace RmlKit::UI {

struct NavigationModel {
    Rml::String view;
    Rml::String crumbParent;
    Rml::String crumb;
    Rml::String crumbDetail;
};

void bind(Rml::DataModelConstructor& model, NavigationModel& state);

}
