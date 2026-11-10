#! The smallest RmlKit app that builds for the desktop and the browser from the same sources.
cmake_minimum_required(VERSION 3.29)

set(PLATFORM "" CACHE STRING "Web builds for a browser; empty builds for the toolchain's own system")
if(PLATFORM STREQUAL "Web")
    include(../cmake/Emsdk.cmake)
endif()

project(RmlKitExample VERSION 1.0.0 LANGUAGES C CXX)

include(../cmake/CompilerOptions.cmake)
include(../cmake/App.cmake)

set(RMLKIT_VIDEO OFF)
set(RMLKIT_APP_ID "dev.marcelk.rmlkitexample")
#! A debug desktop build reads the UI and the assets from these folders, so an edit shows on the next reload.
set(RMLKIT_UI_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
set(RMLKIT_ASSET_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
add_subdirectory(.. rmlkit EXCLUDE_FROM_ALL)

add_executable(RmlKitExample Main.cpp Application.cpp)
target_link_libraries(RmlKitExample PRIVATE RmlKit::RmlKit)
target_compile_options(RmlKitExample PRIVATE -fno-exceptions)
target_compile_definitions(RmlKitExample PRIVATE PRODUCT_NAME="${PROJECT_NAME}")
rmlkit_add_app(RmlKitExample
    UI_DIR "${PROJECT_SOURCE_DIR}"
    ASSET_DIR "${PROJECT_SOURCE_DIR}"
    ICON "${PROJECT_SOURCE_DIR}/assets/icon.svg"
    DESKTOP_COMMENT "An example app built with RmlKit"
    DESKTOP_CATEGORIES "Development;")
