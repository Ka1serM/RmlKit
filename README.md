# RmlKit

A C++23 app shell for [RmlUi](https://github.com/mikke89/RmlUi) desktop and browser apps: the SDL window and frame
loop, the router, the data model, the window chrome, and a set of controls. It has no Unreal code. Verylia and
UniversalUmap use it.

## Dependencies

- CMake 3.29+ and Ninja
- Clang 22+ with libc++, LLVM's runtime (compiler-rt and libunwind) and LLD. The presets choose the compiler. No part
  of GCC is used.
- Linux: the Wayland client and PulseAudio headers
- Windows: [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) with its `bin` folder in PATH
- The browser: Emscripten, which the app installs
- The video element: make, and on a Windows host MSYS2 with make and diffutils, for the FFmpeg build

Git submodules in `external/`, built from source: SDL, RmlUi, FreeType, HarfBuzz, lunasvg (with plutovg), oneTBB, and
FFmpeg (only with `RMLKIT_VIDEO`).

## Build

```sh
git submodule update --init --recursive
cmake --workflow --preset linux-release
```

The presets are `<platform>-<type>`: the platform `linux` or `windows`, the type `debug`, `relwithdebinfo` or
`release`. Each one configures and builds the library in `build-<preset>`. An app builds the browser version.

## Layout

`src/` is the include root. Each header has its source next to it. A file that ends in `Desktop` or `Web` builds only
for that platform.

| Folder | Contents |
| --- | --- |
| `src/RmlKit/UI/Backend/` | `Application`, the SDL window, the OpenGL renderer, the job dispatcher, the router, the message boxes, the tooltip, the file dialogs, the drag and drop, the embedded resources |
| `src/RmlKit/UI/Controls/` | The custom elements: the virtual grid, the spinner, the rename input, the loading decorator, the video element |
| `src/RmlKit/UI/Models/` | `Model`, the `app` data model, with the navigation, the log and the message box |
| `src/RmlKit/Media/` | The FFmpeg video player |
| `src/RmlKit/Core/` | The error type, the UTF-8 paths, and the gaps of the standard libraries |
| `web/` | The Emscripten library of the browser version |
| `tools/` | The comment check and the pre-commit hook |

## Usage

Add RmlKit to the app before the app's own targets, and embed the UI:

```cmake
set(RMLKIT_APP_ID "dev.example.app")
set(RMLKIT_UI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/App")
set(RMLKIT_ASSET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory(external/RmlKit)

add_executable(App Main.cpp)
target_link_libraries(App PRIVATE RmlKit::RmlKit)
rmlkit_add_resources(resources ROOT "${RMLKIT_UI_SOURCE_DIR}" ${ui_files} ROOT "${RMLKIT_ASSET_SOURCE_DIR}" ${asset_files})
```

The window title and the product name are the name and version of the top-level CMake project.

The app derives from `RmlKit::UI::Application`, binds its view models into `getModel()`, and runs from SDL's main
callbacks:

```cpp
class App : public RmlKit::UI::Application {
public:
    std::expected<void, RmlKit::Core::Error> startup() override;
    std::expected<void, RmlKit::Core::Error> shutdown() override;
};
```

The UI has these files and elements:

- `UI/Documents/main.rml`: the main document, with the elements `views` and `tooltip`
- `UI/Views/<name>.rml`: a view that the router shows in `views`
- `assets/fonts/`: the fonts, which load at startup

The browser page sets `Module.rmlkitChoose`, `Module.rmlkitCopy`, `Module.rmlkitShowLinkDrag` and
`Module.rmlkitHideLinkDrag`, and calls `rmlkit_paste` with pasted text.

## Comments

Code is the documentation: use names that explain it. A comment is permitted only for behavior that the code cannot
show, such as an OS quirk, a thread rule or a file format fact. It is one line, and it has a mark: `//!` in C++ and JS,
and `#!` in CMake and Python. `tools/check_comments.py` rejects all other comments.

## License

MIT. See [LICENSE](LICENSE).
