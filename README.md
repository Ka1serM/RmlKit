# RmlKit

A C++23 app shell for [RmlUi](https://github.com/mikke89/RmlUi) apps on the desktop and in the browser: the SDL window
and frame loop, the router, the data model, the window chrome, and a set of controls.

## Usage

```cmake
include(external/RmlKit/cmake/EmbeddedResources.cmake)
set(RMLKIT_APP_ID "dev.example.app")
set(RMLKIT_UI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/App")
set(RMLKIT_ASSET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory(external/RmlKit)

add_executable(App Main.cpp)
target_link_libraries(App PRIVATE RmlKit::RmlKit)
rmlkit_add_resources(resources ROOT "${RMLKIT_UI_SOURCE_DIR}" ${ui_files} ROOT "${RMLKIT_ASSET_SOURCE_DIR}" ${asset_files})
```

```cpp
class App : public RmlKit::UI::Application {
public:
    std::expected<void, RmlKit::Core::Error> startup() override;
    std::expected<void, RmlKit::Core::Error> shutdown() override;
};
```

The app binds its view models into `getModel()`. The UI has `UI/Documents/main.rml` with the elements `views` and
`tooltip`, one `UI/Views/<name>.rml` for each view of the router, and the fonts in `assets/fonts/`. The browser page
sets `Module.rmlkitChoose`, `Module.rmlkitCopy`, `Module.rmlkitShowLinkDrag` and `Module.rmlkitHideLinkDrag`, and
calls `rmlkit_paste` with pasted text.

## Dependencies

- CMake 3.29+ and Ninja
- Clang 22+ with libc++, compiler-rt, libunwind and LLD
- Linux: the Wayland client and PulseAudio headers
- Windows: [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)
- The browser: Emscripten
- `RMLKIT_VIDEO`: make, and on Windows MSYS2 with make and diffutils

Git submodules in `external/`: SDL, RmlUi, FreeType, HarfBuzz, lunasvg, oneTBB, and FFmpeg (only with `RMLKIT_VIDEO`).
