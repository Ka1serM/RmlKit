#include "RmlKit/UI/Backend/RmlRuntime.h"

#include <format>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Factory.h>

#include "RmlKit/UI/Backend/EmbeddedResources.h"
#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

RmlRuntime::RmlRuntime(Window& window)
    : system(window.getNative()), textureLoading(window.getRenderer())
#if RMLKIT_VIDEO
      ,
      video(videoConverter, videos)
#endif
{
    Rml::SetSystemInterface(&system);
    Rml::SetRenderInterface(&window.getRenderer());
    Rml::SetFileInterface(&files);
    Rml::SetTextInputHandler(&textInput);
    Rml::SetFontEngineInterface(&fonts);
    if (!Rml::Initialise())
        fatal("RmlUi failed to initialize");
    Rml::Factory::RegisterElementInstancer("virtual-grid", &virtualGrid);
    Rml::Factory::RegisterElementInstancer("spinner", &spinner);
    Rml::Factory::RegisterElementInstancer("rename-input", &renameInput);
    Rml::Factory::RegisterDecoratorInstancer("texture-loading", &textureLoading);
#if RMLKIT_VIDEO
    Rml::Factory::RegisterElementInstancer("video", &video);
#endif

    for (const std::string& font : EmbeddedResources::list("UI/RmlKit/Fonts")) {
        if (!font.ends_with(".ttf"))
            continue;
        const Rml::String path = "UI/RmlKit/Fonts/" + font;
        if (!Rml::LoadFontFace(path))
            fatal(std::format("RmlUi cannot load the font {}", path));
    }
}

RmlRuntime::~RmlRuntime() {
    Rml::Shutdown();
}

}
