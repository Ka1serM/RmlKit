#pragma once

#include <RmlUi/Core/Elements/ElementFormControlInput.h>

namespace RmlKit::UI {

class RenameInput final : public Rml::ElementFormControlInput {
public:
    explicit RenameInput(const Rml::String& tag) : Rml::ElementFormControlInput(tag) {}

protected:
    void OnPropertyChange(const Rml::PropertyIdSet& changedProperties) override;
};

}
