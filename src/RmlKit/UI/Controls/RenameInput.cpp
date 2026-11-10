#include "RmlKit/UI/Controls/RenameInput.h"

#include <RmlUi/Core/ID.h>
#include <RmlUi/Core/PropertyIdSet.h>

namespace RmlKit::UI {

void RenameInput::OnPropertyChange(const Rml::PropertyIdSet& changedProperties) {
    Rml::ElementFormControlInput::OnPropertyChange(changedProperties);
    if (!changedProperties.Contains(Rml::PropertyId::Display))
        return;
    if (!IsVisible()) {
        Blur();
        return;
    }
    Focus();
    Select();
}

}
