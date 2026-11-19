#include "RmlKit/UI/Backend/SecondaryWindow.h"

#include <format>

namespace RmlKit::UI {

SecondaryWindow::SecondaryWindow(NoorRHI::Device& device, const WindowOptions& options)
    : window(device, options), view(window, std::format("window-{}", window.getId()), [this] { open = false; }) {}

}
