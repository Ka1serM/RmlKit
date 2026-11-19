#include "RmlKit/UI/Backend/SecondaryWindow.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace RmlKit::UI {

namespace {

Rml::String nextContextName() {
    static int count = 0;
    return std::format("overlay-{}", ++count);
}

Rml::Rectanglei centeredArea(const Window& main, const WindowOptions& options) {
    const float density = SDL_GetWindowPixelDensity(main.getNative());
    const Rml::Vector2i bounds = main.pixelSize();
    const Rml::Vector2i size{std::min(static_cast<int>(std::lround(static_cast<float>(options.width) * density)), bounds.x),
                             std::min(static_cast<int>(std::lround(static_cast<float>(options.height) * density)), bounds.y)};
    return Rml::Rectanglei::FromPositionSize((bounds - size) / 2, size);
}

}

SecondaryWindow::SecondaryWindow(Window& main, const WindowOptions& options)
    : window(main), area(centeredArea(main, options)), view(main, nextContextName(), [this] { open = false; }) {
    view.setOverlaySize(area.Size());
    Model& model = view.getModel();
    model.windowButtons = true;
    model.windowButtonsEnd = {"close"};
    model.dirty("windowButtons");
    model.dirty("windowButtonsEnd");
}

void SecondaryWindow::move(const Rml::Vector2i offset) {
    const Rml::Vector2i limit = window.pixelSize() - area.Size();
    const Rml::Vector2i moved = area.Position() + offset;
    const Rml::Vector2i position{std::clamp(moved.x, 0, std::max(limit.x, 0)), std::clamp(moved.y, 0, std::max(limit.y, 0))};
    area = Rml::Rectanglei::FromPositionSize(position, area.Size());
}

}
