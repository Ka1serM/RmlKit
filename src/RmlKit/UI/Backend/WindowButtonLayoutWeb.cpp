#include "RmlKit/UI/Backend/WindowButtonLayout.h"

namespace RmlKit::UI {

const WindowButtonLayout& getWindowButtonLayout() {
    static const WindowButtonLayout layout{"windows", {}, {}};
    return layout;
}

}
