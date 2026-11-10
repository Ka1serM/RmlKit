#include "RmlKit/UI/Models/MessageBoxModel.h"

namespace RmlKit::UI {

void bind(Rml::DataModelConstructor& model, MessageBoxModel& state) {
    auto screen = model.RegisterStruct<MessageBoxModel>();
    screen.RegisterMember("level", &MessageBoxModel::level);
    screen.RegisterMember("icon", &MessageBoxModel::icon);
    screen.RegisterMember("title", &MessageBoxModel::title);
    screen.RegisterMember("text", &MessageBoxModel::text);
    model.Bind("messageBox", &state);
}

}
