# RmlKit

A C++23 app shell for [RmlUi](https://github.com/mikke89/RmlUi) apps on the desktop and in the browser: the SDL window
and frame loop, the router, the data model, the window chrome, and a set of controls.

## Usage

```cmake
cmake_minimum_required(VERSION 3.29)
set(PLATFORM "" CACHE STRING "Web builds for a browser")
if(PLATFORM STREQUAL "Web")
    include(external/RmlKit/cmake/Emsdk.cmake)
endif()
project(App VERSION 1.0.0 LANGUAGES C CXX)

include(external/RmlKit/cmake/CompilerOptions.cmake)
include(external/RmlKit/cmake/App.cmake)
set(RMLKIT_APP_ID "dev.example.app")
set(RMLKIT_UI_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/App")
set(RMLKIT_ASSET_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
add_subdirectory(external/RmlKit)

add_executable(App src/App/App.cpp)
target_include_directories(App PRIVATE src/App)
target_link_libraries(App PRIVATE RmlKit::RmlKit)
rmlkit_add_app(App APPLICATION App APPLICATION_HEADER "App.h" UI_DIR "${RMLKIT_UI_SOURCE_DIR}" ASSET_DIR "${RMLKIT_ASSET_SOURCE_DIR}" ICON "${PROJECT_SOURCE_DIR}/assets/icon.svg")
```

```cpp
class App : public RmlKit::UI::Application {
public:
    std::expected<void, RmlKit::Core::Error> startup() override;
    std::expected<void, RmlKit::Core::Error> shutdown() override;
};
```

The app binds its view models into `getModel()`. The UI has `UI/Documents/main.rml` with the elements `views` and
`tooltip`, and one `UI/Views/<name>.rml` for each view of the router. RmlKit embeds its own `UI/RmlKit/`: the
base styles, the message box, and the Google Sans fonts. A document links them first, such as
`../RmlKit/Styles/theme.rcss`, then the app's styles.

[`example/`](example) is a complete app: a counter, a text field, the message box, the log, and a background job.

```sh
cd example
cmake --preset linux-debug && cmake --build build-linux-debug && ./build-linux-debug/RmlKitExample
cmake --preset web-debug && cmake --build build-web-debug && python3 ../tools/serve_web.py build-web-debug
```

### rmlkit_add_app

| Argument | Meaning |
| --- | --- |
| `APPLICATION` | The app's subclass of `RmlKit::UI::Application`, such as `Example::Application`. RmlKit's `main` constructs it. Required. |
| `APPLICATION_HEADER` | The header that declares `APPLICATION`, as the app includes it. Required. |
| `UI_DIR` | The folder that holds `UI/`. Its `.rml` and `.rcss` files ship with the app. Required. |
| `ASSET_DIR` | The folder that holds `assets/`. Every file in it ships with the app. |
| `RESOURCES` | More files, as `ROOT <folder> <file>...`. Each ships at its path below its `ROOT`. |
| `DEPENDS` | The targets that make the `RESOURCES` files. |
| `ICON` | The app icon, `.svg` or `.png`: the Linux launcher icon and the web page icon. |
| `TOUCH_ICON` | A 180 to 256 pixel `.png` for the home screen of a phone. |
| `DESKTOP_COMMENT` | The Linux launcher's tooltip. |
| `DESKTOP_CATEGORIES` | The Linux launcher's menu categories, such as `Graphics;3DGraphics;`. |
| `WEB_PRE_JS` | A `--pre-js` file. It can set `Module.rmlkitStart(status)`, which must call `Module.callMain()`. |
| `WEB_LIBRARY_JS` | A `--js-library` file with the app's own JavaScript functions. |

The desktop build embeds the files into the executable. The web build preloads them into `index.data`, and its page
comes from `web/shell.html`.

### Kit views and styles

The view `logs` comes from RmlKit: `UI/RmlKit/Views/logs.rml`, with the controller that the app's `Application` already
owns. The main document links `UI/RmlKit/Styles/logs.rcss`, and `onboarding.rcss` and `settings.rcss` when it has those
pages. The icons in `UI/RmlKit/Icons` have root paths, such as `/UI/RmlKit/Icons/icon-check.svg`.

### Linux launcher

With an `ICON`, the app writes `~/.local/share/applications/<RMLKIT_APP_ID>.desktop` and the icon at each start, so the
launcher follows the executable when it moves. It leaves a launcher alone once its `X-<project>-Generated=true` line
is gone, and writes nothing in a Flatpak.

### Browser

`cmake/Emsdk.cmake` installs the pinned Emscripten into the `external/emsdk` submodule on the first configure.
`tools/serve_web.py` serves a build with the headers that threads need. The page sets `Module.rmlkitCopy`,
`Module.rmlkitShowLinkDrag` and `Module.rmlkitHideLinkDrag`; an app that opens files sets `Module.rmlkitChoose` in its
`WEB_PRE_JS`.

## Dependencies

- CMake 3.29+ and Ninja
- Clang 22+ with libc++, compiler-rt, libunwind and LLD
- Linux: the Wayland client and PulseAudio headers
- Windows: [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)
- The browser: Python 3 and Git, for the Emscripten that `cmake/Emsdk.cmake` installs
- `RMLKIT_VIDEO` (off by default; it builds the video player and the `<video>` element): make, and on Windows MSYS2 with make and diffutils

Git submodules in `external/`: emsdk (only for the browser), SDL, RmlUi, FreeType, HarfBuzz, lunasvg, oneTBB, and FFmpeg (only with `RMLKIT_VIDEO`).
