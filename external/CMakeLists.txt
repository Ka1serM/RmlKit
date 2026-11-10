# The dependencies, built from source as static libraries without warnings. Each builds only the parts that RmlKit
# uses.

# A third-party project or target compiles without warnings: they are not ours to fix.
set(no_warnings -w)
function(add_external_subdirectory source)
    get_directory_property(options COMPILE_OPTIONS)
    add_compile_options(${no_warnings})
    add_subdirectory(${source} ${ARGN} EXCLUDE_FROM_ALL SYSTEM)
    set_directory_properties(PROPERTIES COMPILE_OPTIONS "${options}")
endfunction()

set(submodules freetype harfbuzz lunasvg oneTBB RmlUi SDL)
if(RMLKIT_VIDEO)
    list(APPEND submodules FFmpeg)
endif()
foreach(submodule ${submodules})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${submodule}/.git")
        message(FATAL_ERROR "external/${submodule} is missing. Run: git submodule update --init --recursive")
    endif()
endforeach()

# SDL gives the window, the input, the audio output, the folder dialogs, and the OpenGL context. RmlUi draws through
# its OpenGL 3 renderer, so SDL's renderer, GPU API, and other video backends are off. Linux uses only the current
# backends: Wayland, not X11, and PulseAudio, which PipeWire systems also serve (pipewire-pulse).
foreach(sdl_option SHARED TESTS TEST_LIBRARY EXAMPLES INSTALL CAMERA GPU RENDER VULKAN HAPTIC JOYSTICK HIDAPI SENSOR POWER
        TRAY X11 WAYLAND_LIBDECOR DUMMYVIDEO DUMMYAUDIO DISKAUDIO DUMMYCAMERA OFFSCREEN KMSDRM VIRTUAL_JOYSTICK PIPEWIRE
        ALSA OSS JACK SNDIO IBUS FRIBIDI LIBTHAI RPI ROCKCHIP VIVANTE OPENVR)
    set(SDL_${sdl_option} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(sdl_option STATIC VIDEO OPENGL AUDIO DIALOG)
    set(SDL_${sdl_option} ON CACHE BOOL "" FORCE)
endforeach()
if(PLATFORM_WEB)
    set(SDL_OPENGLES ON CACHE BOOL "" FORCE)
    set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
endif()
# Without the PulseAudio headers, SDL silently builds no audio driver, and SDL_Init fails at startup.
if(PLATFORM_LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libpulse REQUIRED libpulse)
endif()
add_external_subdirectory(SDL)

# FreeType, the TrueType core only. The function scope keeps an earlier find_package result, such as ZLIB_FOUND, from
# enabling a disabled codec.
function(add_freetype)
    foreach(codec ZLIB BZIP2 PNG HARFBUZZ BROTLI)
        set(FT_DISABLE_${codec} ON CACHE BOOL "" FORCE)
        set(${codec}_FOUND OFF)
    endforeach()
    add_external_subdirectory(freetype)
endfunction()
add_freetype()
add_library(Freetype::Freetype ALIAS freetype)

# HarfBuzz shapes the text: kerning, ligatures, and the other OpenType features. The sources that the amalgamated
# harfbuzz.cc includes compile one by one, in parallel. The HAVE_FT_* functions let HarfBuzz read and set the
# coordinates of a variable font.
file(STRINGS harfbuzz/src/harfbuzz.cc harfbuzz_includes REGEX "^#include \"hb-.*\\.cc\"")
list(TRANSFORM harfbuzz_includes REPLACE "^#include \"(.*)\"$" "${CMAKE_CURRENT_SOURCE_DIR}/harfbuzz/src/\\1")
add_library(harfbuzz STATIC ${harfbuzz_includes} harfbuzz/src/OT/Var/VARC/VARC.cc)
target_include_directories(harfbuzz SYSTEM PUBLIC harfbuzz/src)
target_compile_definitions(harfbuzz PRIVATE HAVE_FREETYPE HAVE_FT_GET_VAR_BLEND_COORDINATES HAVE_FT_SET_VAR_BLEND_COORDINATES
    HAVE_FT_DONE_MM_VAR HAVE_FT_GET_TRANSFORM)
target_link_libraries(harfbuzz PUBLIC freetype)
target_compile_options(harfbuzz PRIVATE ${no_warnings})

# lunasvg draws the SVG icons. It brings its own plutovg, which also decodes the PNG and JPEG images.
set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_external_subdirectory(lunasvg)

# RmlUi takes the FreeType target above, not a system package.
set(CMAKE_DISABLE_FIND_PACKAGE_Freetype ON)
set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
set(RMLUI_SVG_PLUGIN ON CACHE BOOL "" FORCE)
set(RMLUI_PRECOMPILED_HEADERS ON CACHE BOOL "" FORCE)
foreach(rmlui_option SAMPLES LOTTIE_PLUGIN LUA_BINDINGS)
    set(RMLUI_${rmlui_option} OFF CACHE BOOL "" FORCE)
endforeach()
add_external_subdirectory(RmlUi)
# RmlUi's SDL platform and OpenGL 3 renderer, and the HarfBuzz font engine of its sample (RmlUi Core has none). The font
# engine uses private RmlUi Core headers. The renderer draws its layers with four MSAA samples.
file(GLOB rmlui_harfbuzz_sources RmlUi/Samples/basic/harfbuzz/src/*.cpp)
list(FILTER rmlui_harfbuzz_sources EXCLUDE REGEX "/main\\.cpp$")
add_library(rmlui_backend STATIC ${rmlui_harfbuzz_sources} RmlUi/Backends/RmlUi_Platform_SDL.cpp
    RmlUi/Backends/RmlUi_Renderer_GL3.cpp)
target_include_directories(rmlui_backend SYSTEM PUBLIC RmlUi/Backends RmlUi/Samples/basic/harfbuzz/src PRIVATE RmlUi/Source/Core)
target_compile_definitions(rmlui_backend PUBLIC RMLUI_SDL_VERSION_MAJOR=3 RMLUI_NUM_MSAA_SAMPLES=4)
target_link_libraries(rmlui_backend PUBLIC RmlUi::Core RmlUi::Debugger harfbuzz SDL3::SDL3-static)
target_compile_options(rmlui_backend PRIVATE ${no_warnings})

# oneTBB runs the background jobs. An app that adds oneTBB first keeps its own.
if(NOT TARGET TBB::tbb)
    set(TBB_TEST OFF CACHE BOOL "" FORCE)
    set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(TBB_STRICT OFF CACHE BOOL "" FORCE)
    set(TBBMALLOC_BUILD OFF CACHE BOOL "" FORCE)
    set(TBB_INSTALL OFF CACHE BOOL "" FORCE)
    #! TBBBind and the Thread Composability Manager need hwloc, and a second configure fails without it.
    set(TCM_BUILD OFF CACHE BOOL "" FORCE)
    set(TBB_DISABLE_HWLOC_AUTOMATIC_SEARCH ON CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF)
    add_external_subdirectory(oneTBB)
endif()

if(RMLKIT_VIDEO)
    # FFmpeg plays the videos of the video element: the MP4 demuxer, H.264 and AAC decoded in software on frame threads, and swscale, which
    # converts frames of other pixel formats to YUV 4:2:0. A static LGPL build with FFmpeg's own configure and make, as
    # FFmpeg documents it, with the compiler of the build. On a Windows host, the shell and make of MSYS2 run the build.
    include(ExternalProject)
    set(ffmpeg_options --enable-static --disable-shared --disable-programs --disable-doc --disable-debug --disable-autodetect
        --disable-network --disable-x86asm --disable-everything --disable-avdevice --disable-avfilter --disable-swresample
        --enable-protocol=file --enable-demuxer=mov --enable-parser=h264,aac --enable-decoder=h264,aac)
    function(validate_ffmpeg_make result candidate)
        if(NOT CMAKE_HOST_WIN32)
            return()
        endif()
        cmake_path(GET candidate PARENT_PATH directory)
        if(NOT EXISTS "${directory}/bash.exe" OR NOT EXISTS "${directory}/cmp.exe")
            set(${result} FALSE PARENT_SCOPE)
        endif()
    endfunction()
    find_program(FFMPEG_MAKE make VALIDATOR validate_ffmpeg_make)
    if(NOT FFMPEG_MAKE)
        message(FATAL_ERROR "FFmpeg's build needs make in PATH. On a Windows host, it needs MSYS2 with make and diffutils: "
            "bash.exe and cmp.exe next to make.exe.")
    endif()
    # FFmpeg's configure ignores CC and calls gcc, so it takes the compiler, its cache, the archive tools, and the CPU target
    # here.
    if(CCACHE)
        set(ffmpeg_cc "${CCACHE} ${CMAKE_C_COMPILER}")
    else()
        set(ffmpeg_cc "${CMAKE_C_COMPILER}")
    endif()
    if(CMAKE_HOST_WIN32)
        # The bash next to MSYS2's make, not another bash in PATH, such as the one of WSL. No login shell: it would reset
        # PATH, and llvm-mingw would be missing from it.
        cmake_path(GET FFMPEG_MAKE PARENT_PATH msys2_bin)
        set(ffmpeg_shell "${msys2_bin}/bash.exe" --noprofile --norc -c)
        set(ffmpeg_path "export PATH=/usr/bin:$PATH && ")
    else()
        find_program(BASH bash REQUIRED)
        set(ffmpeg_shell "${BASH}" -c)
        set(ffmpeg_path "")
    endif()
    # A cross build compiles FFmpeg's host checks with gcc unless told, and a system with Clang alone has none.
    if(PLATFORM_WEB OR PLATFORM_WINDOWS)
        find_program(FFMPEG_HOST_CC NAMES cc clang REQUIRED)
        list(APPEND ffmpeg_options "--host-cc=${FFMPEG_HOST_CC}")
    endif()
    if(PLATFORM_WEB)
        string(APPEND ffmpeg_path "export EM_CONFIG='${EMSDK_ROOT}/.emscripten' && ")
        list(APPEND ffmpeg_options --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic
            "--cc=${EMSCRIPTEN_ROOT_PATH}/emcc" "--cxx=${EMSCRIPTEN_ROOT_PATH}/em++" "--ar=${EMSCRIPTEN_ROOT_PATH}/emar"
            "--ranlib=${EMSCRIPTEN_ROOT_PATH}/emranlib" "--nm=${EMSCRIPTEN_ROOT_PATH}/../bin/llvm-nm" --disable-asm
            --disable-inline-asm --disable-stripping "--extra-cflags=-pthread -msimd128 -O3" "--extra-ldflags=-pthread")
    else()
        # The configure for Windows is a cross build also on a Windows host: MSYS2's uname names another target.
        if(PLATFORM_WINDOWS)
            list(APPEND ffmpeg_options --enable-cross-compile --target-os=mingw32 --arch=x86_64)
        endif()
        # The Linux executable is PIE. Configure detects PIC from the compiler's defaults, and that detection can differ
        # between runs, while make keeps the objects of an earlier configure.
        if(PLATFORM_LINUX)
            list(APPEND ffmpeg_options --enable-pic "--extra-ldflags=--rtlib=compiler-rt --unwindlib=libunwind")
        endif()
        list(APPEND ffmpeg_options "--cc=${ffmpeg_cc}" "--ar=${CMAKE_AR}" "--ranlib=${CMAKE_RANLIB}" "--nm=${CMAKE_NM}"
            "--extra-cflags=-march=x86-64-v3 -ffunction-sections -fdata-sections")
    endif()
    list(TRANSFORM ffmpeg_options PREPEND "'")
    list(TRANSFORM ffmpeg_options APPEND "'")
    list(JOIN ffmpeg_options " " ffmpeg_options)
    cmake_host_system_information(RESULT ffmpeg_jobs QUERY NUMBER_OF_LOGICAL_CORES)
    set(ffmpeg_prefix "${CMAKE_CURRENT_BINARY_DIR}/FFmpeg/install")
    set(ffmpeg_libraries avformat avcodec swscale avutil)
    set(ffmpeg_files)
    foreach(library ${ffmpeg_libraries})
        list(APPEND ffmpeg_files "${ffmpeg_prefix}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${library}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endforeach()
    ExternalProject_Add(ffmpeg
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/FFmpeg"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/FFmpeg/build"
        INSTALL_DIR "${ffmpeg_prefix}"
        CONFIGURE_COMMAND ${ffmpeg_shell} "${ffmpeg_path}'<SOURCE_DIR>/configure' '--prefix=<INSTALL_DIR>' ${ffmpeg_options}"
        BUILD_COMMAND ${ffmpeg_shell} "${ffmpeg_path}make -j${ffmpeg_jobs}"
        INSTALL_COMMAND ${ffmpeg_shell} "${ffmpeg_path}make install"
        BUILD_BYPRODUCTS ${ffmpeg_files})
    file(MAKE_DIRECTORY "${ffmpeg_prefix}/include")
    add_library(ffmpeg_libraries INTERFACE)
    add_dependencies(ffmpeg_libraries ffmpeg)
    target_include_directories(ffmpeg_libraries SYSTEM INTERFACE "${ffmpeg_prefix}/include")
    # avutil generates random seeds with BCryptGenRandom on Windows.
    target_link_libraries(ffmpeg_libraries INTERFACE ${ffmpeg_files} "$<$<PLATFORM_ID:Windows>:bcrypt>")
    add_library(FFmpeg::FFmpeg ALIAS ffmpeg_libraries)
endif()
