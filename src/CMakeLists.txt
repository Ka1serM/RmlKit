file(GLOB_RECURSE rmlkit_sources CONFIGURE_DEPENDS RmlKit/*.cpp)
#! rmlkit_add_resources adds the resource reader with the index of the app's resources.
list(FILTER rmlkit_sources EXCLUDE REGEX "/EmbeddedResourcesDesktop\\.cpp$")
if(PLATFORM_WEB)
    list(FILTER rmlkit_sources EXCLUDE REGEX "Desktop\\.cpp$")
else()
    list(FILTER rmlkit_sources EXCLUDE REGEX "Web\\.cpp$")
endif()
if(NOT RMLKIT_VIDEO)
    list(FILTER rmlkit_sources EXCLUDE REGEX "/Video[A-Za-z]*\\.cpp$")
endif()

add_library(RmlKit STATIC ${rmlkit_sources})
add_library(RmlKit::RmlKit ALIAS RmlKit)
target_include_directories(RmlKit PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(RmlKit PUBLIC rmlui_backend plutovg::plutovg TBB::tbb)
target_compile_options(RmlKit PRIVATE -fno-exceptions)
target_compile_definitions(RmlKit
    PUBLIC PLATFORM_WINDOWS=$<BOOL:${PLATFORM_WINDOWS}> PLATFORM_LINUX=$<BOOL:${PLATFORM_LINUX}> PLATFORM_WEB=$<BOOL:${PLATFORM_WEB}>
        RMLKIT_VIDEO=$<BOOL:${RMLKIT_VIDEO}>
    PRIVATE PRODUCT_NAME="${CMAKE_PROJECT_NAME}" PRODUCT_VERSION="${CMAKE_PROJECT_VERSION}" APP_ID="${RMLKIT_APP_ID}"
        UI_SOURCE_DIR="${RMLKIT_UI_SOURCE_DIR}" ASSET_SOURCE_DIR="${RMLKIT_ASSET_SOURCE_DIR}")
target_precompile_headers(RmlKit PRIVATE <RmlUi/Core/Types.h> <RmlUi/Core/DataModelHandle.h> <algorithm> <expected> <format>
    <memory> <optional> <string> <string_view> <utility> <vector> "${CMAKE_CURRENT_SOURCE_DIR}/RmlKit/Core/StdGaps.h")
if(RMLKIT_VIDEO)
    target_link_libraries(RmlKit PUBLIC FFmpeg::FFmpeg)
endif()

if(PLATFORM_WINDOWS)
    #! The OpenGL loader includes <windows.h>, whose min and max macros break std::min.
    target_compile_definitions(RmlKit PUBLIC NOMINMAX)
    target_link_libraries(RmlKit PUBLIC dwmapi ole32 shell32)
elseif(PLATFORM_LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
    target_link_libraries(RmlKit PUBLIC PkgConfig::WAYLAND_CLIENT)
elseif(PLATFORM_WEB)
    target_link_options(RmlKit INTERFACE "--js-library=${PROJECT_SOURCE_DIR}/web/library.js")
    set_property(TARGET RmlKit APPEND PROPERTY INTERFACE_LINK_DEPENDS "${PROJECT_SOURCE_DIR}/web/library.js")
endif()
