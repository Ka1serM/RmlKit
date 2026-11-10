#pragma once

#include <FontEngineInterfaceHarfBuzz.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi_Platform_SDL.h>

#include "RmlKit/UI/Backend/EmbeddedFileInterface.h"
#include "RmlKit/UI/Backend/LogRoute.h"
#include "RmlKit/UI/Backend/SourceFileInterface.h"
#include "RmlKit/UI/Backend/Window.h"
#include "RmlKit/UI/Controls/RenameInput.h"
#include "RmlKit/UI/Controls/SpinnerElement.h"
#include "RmlKit/UI/Controls/SplitterElement.h"
#include "RmlKit/UI/Controls/TextureLoadingDecorator.h"
#include "RmlKit/UI/Controls/VirtualGridElement.h"

#if RMLKIT_VIDEO
#include "RmlKit/UI/Backend/VideoConverter.h"
#include "RmlKit/UI/Backend/VideoStore.h"
#include "RmlKit/UI/Controls/VideoElement.h"
#endif

namespace RmlKit::UI {

class RmlRuntime {
public:
    explicit RmlRuntime(Window& window);
    ~RmlRuntime();
    RmlRuntime(const RmlRuntime&) = delete;
    RmlRuntime& operator=(const RmlRuntime&) = delete;

    TextInputMethodEditor_SDL& getTextInput() { return textInput; }
#if RMLKIT_VIDEO
    VideoStore& getVideos() { return videos; }
#endif
    LogSystemInterface& getSystem() { return system; }

private:
    LogSystemInterface system;
#if defined(NDEBUG) || PLATFORM_WEB
    EmbeddedFileInterface files;
#else
    SourceFileInterface files;
#endif
    TextInputMethodEditor_SDL textInput;
    FontEngineInterfaceHarfBuzz fonts;
    Rml::ElementInstancerGeneric<VirtualGridElement> virtualGrid;
    Rml::ElementInstancerGeneric<SpinnerElement> spinner;
    Rml::ElementInstancerGeneric<SplitterElement> splitter;
    Rml::ElementInstancerGeneric<RenameInput> renameInput;
    TextureLoadingDecoratorInstancer textureLoading;
#if RMLKIT_VIDEO
    VideoConverter videoConverter;
    VideoStore videos;
    VideoElementInstancer video;
#endif
};

}
