#include "RmlKit/UI/Models/NavigationModel.h"

namespace RmlKit::UI {

void bind(Rml::DataModelConstructor& model, NavigationModel& state) {
    auto navigation = model.RegisterStruct<NavigationModel>();
    navigation.RegisterMember("view", &NavigationModel::view);
    navigation.RegisterMember("crumbParent", &NavigationModel::crumbParent);
    navigation.RegisterMember("crumb", &NavigationModel::crumb);
    navigation.RegisterMember("crumbDetail", &NavigationModel::crumbDetail);
    model.Bind("navigation", &state);
}

}
