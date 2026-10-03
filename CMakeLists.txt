cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(Storyland VERSION 1.1.5.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(STORYLAND_ARCHIVE_TESTS_ONLY "Build portable archive inspection and parser tests without the Windows UI" OFF)
if(STORYLAND_ARCHIVE_TESTS_ONLY)
    find_package(ZLIB REQUIRED)
    enable_testing()
    set(STORYLAND_ARCHIVE_SUPPORT
        src/leeds/storyland_model.cpp
        src/leeds/leeds_texture.cpp
        src/validation/storyland_dma_validator.cpp
    )
    add_library(storyland_archive_support STATIC ${STORYLAND_ARCHIVE_SUPPORT})
    target_include_directories(storyland_archive_support PUBLIC src/storyland src/leeds src/validation)
    target_link_libraries(storyland_archive_support PUBLIC ZLIB::ZLIB)
    add_executable(archive_inspect tests/archive_inspect.cpp src/leeds/storyland_archive.cpp)
    target_link_libraries(archive_inspect PRIVATE storyland_archive_support)
    add_executable(loose_inspect tests/loose_inspect.cpp)
    target_link_libraries(loose_inspect PRIVATE storyland_archive_support)
    add_executable(world_parser_tests tests/world_parser_tests.cpp)
    target_link_libraries(world_parser_tests PRIVATE storyland_archive_support)
    add_test(NAME world_parser_tests COMMAND world_parser_tests)
    add_executable(trilogy_format_tests tests/trilogy_format_tests.cpp)
    target_link_libraries(trilogy_format_tests PRIVATE storyland_archive_support)
    add_test(NAME trilogy_format_tests COMMAND trilogy_format_tests)
    return()
endif()
if(WIN32)
    enable_language(RC)
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8)
endif()

if(WIN32 AND DEFINED VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET MATCHES "-static$")
    message(FATAL_ERROR
        "Storyland requires a static vcpkg triplet. Configure with "
        "-DVCPKG_TARGET_TRIPLET=x64-windows-static."
    )
endif()

# Storyland is distributed as a portable executable.  With the static vcpkg
# triplet selected by build_msvc.bat, this resolves to zlib.lib rather than the
# zlib1.dll import library.
set(ZLIB_USE_STATIC_LIBS ON)
find_package(ZLIB REQUIRED)

# Do not use vcpkg's FindFFMPEG.cmake here.  With x64-windows-static,
# FFmpeg 8.x records Windows SDK/system libraries (for example ncrypt) as
# package dependencies.  The wrapper then tries to locate those with
# find_library() and can fail during configure even though they are normal
# Windows import libraries supplied by the SDK.  Locate only the four vcpkg
# FFmpeg libraries Storyland actually consumes and link the Windows
# dependencies by their canonical linker names.
find_path(STORYLAND_FFMPEG_INCLUDE_DIR
    NAMES libavcodec/avcodec.h
    REQUIRED
)
# Visual Studio is a multi-config generator.  A plain find_library() can pick a
# debug vcpkg FFmpeg archive during configure and then feed it into a Release
# link, which pulls LIBCMTD into an otherwise Release executable.  Resolve the
# release/debug libraries from the same vcpkg triplet root as the headers and
# select them per configuration.
get_filename_component(STORYLAND_VCPKG_TRIPLET_ROOT "${STORYLAND_FFMPEG_INCLUDE_DIR}" DIRECTORY)

function(storyland_find_ffmpeg_library out_release out_debug basename)
    find_library(${out_release}
        NAMES ${basename}
        PATHS "${STORYLAND_VCPKG_TRIPLET_ROOT}/lib"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(${out_debug}
        NAMES ${basename}
        PATHS "${STORYLAND_VCPKG_TRIPLET_ROOT}/debug/lib"
        NO_DEFAULT_PATH
    )
    if(NOT ${out_debug})
        set(${out_debug} "${${out_release}}" PARENT_SCOPE)
    else()
        set(${out_debug} "${${out_debug}}" PARENT_SCOPE)
    endif()
    set(${out_release} "${${out_release}}" PARENT_SCOPE)
endfunction()

storyland_find_ffmpeg_library(STORYLAND_AVCODEC_RELEASE STORYLAND_AVCODEC_DEBUG avcodec)
storyland_find_ffmpeg_library(STORYLAND_AVFORMAT_RELEASE STORYLAND_AVFORMAT_DEBUG avformat)
storyland_find_ffmpeg_library(STORYLAND_AVUTIL_RELEASE STORYLAND_AVUTIL_DEBUG avutil)
storyland_find_ffmpeg_library(STORYLAND_SWSCALE_RELEASE STORYLAND_SWSCALE_DEBUG swscale)

set(STORYLAND_FFMPEG_LIBRARIES
    optimized ${STORYLAND_AVFORMAT_RELEASE} debug ${STORYLAND_AVFORMAT_DEBUG}
    optimized ${STORYLAND_AVCODEC_RELEASE} debug ${STORYLAND_AVCODEC_DEBUG}
    optimized ${STORYLAND_SWSCALE_RELEASE} debug ${STORYLAND_SWSCALE_DEBUG}
    optimized ${STORYLAND_AVUTIL_RELEASE} debug ${STORYLAND_AVUTIL_DEBUG}
)

if(WIN32 AND DEFINED ZLIB_LIBRARY_RELEASE)
    file(TO_CMAKE_PATH "${ZLIB_LIBRARY_RELEASE}" STORYLAND_ZLIB_RELEASE_PATH)
    if(STORYLAND_ZLIB_RELEASE_PATH MATCHES "/x64-windows/lib/")
        message(FATAL_ERROR
            "CMake selected dynamic-triplet zlib: ${STORYLAND_ZLIB_RELEASE_PATH}. "
            "Use the clean build-portable directory through build_msvc.bat."
        )
    endif()
endif()

set(STORYLAND_RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows/Storyland.rc")
set_source_files_properties(
    "${STORYLAND_RESOURCE_FILE}"
    PROPERTIES
        LANGUAGE RC
        VS_TOOL_OVERRIDE "ResourceCompile"
)

add_executable(Storyland WIN32
    src/storyland/main.cpp
    src/storyland/storyland_analysis_graph.cpp
    src/storyland/storyland_analysis_graph.h
    src/storyland/storyland_opengl_renderer.cpp
    src/storyland/storyland_opengl_renderer.h
    src/shaders/storyland_shaders.cpp
    src/shaders/storyland_shaders.h
    src/storyland/storyland_sky.cpp
    src/storyland/storyland_sky.h
    src/storyland/storyland_scm.cpp
    src/storyland/storyland_scm.h
    src/storyland/storyland_media.cpp
    src/storyland/storyland_media.h
    src/storyland/storyland_atomic_io.h

    src/leeds/leeds_texture.cpp
    src/leeds/leeds_texture.h
    src/leeds/storyland_archive.cpp
    src/leeds/storyland_archive.h
    src/leeds/storyland_dtz.cpp
    src/leeds/storyland_dtz.h
    src/leeds/storyland_model.cpp
    src/leeds/storyland_model.h
    src/leeds/storyland_anim.cpp
    src/leeds/storyland_anim.h
    src/leeds/storyland_wbl.cpp
    src/leeds/storyland_wbl.h

    src/validation/storyland_dma_validator.cpp
    src/validation/storyland_dma_validator.h
    src/validation/storyland_psp_validator.cpp
    src/validation/storyland_psp_validator.h
    src/validation/storyland_model_validator.cpp
    src/validation/storyland_model_validator.h

    src/platform/windows/wic_image.cpp
    src/platform/windows/wic_image.h
    src/platform/windows/resource.h
    "${STORYLAND_RESOURCE_FILE}"
)

target_include_directories(Storyland PRIVATE
    src/storyland
    src/thirdparty
    src/shaders
    src/leeds
    src/validation
    src/platform/windows
    ${STORYLAND_FFMPEG_INCLUDE_DIR}
)
target_link_libraries(Storyland PRIVATE
    ZLIB::ZLIB
    ${STORYLAND_FFMPEG_LIBRARIES}
    secur32
    ncrypt
    crypt32
    ws2_32
    strmiids
    bcrypt
    windowscodecs
    ole32
    user32
    comctl32
    comdlg32
    shell32
    advapi32
    opengl32
    gdi32
    dwmapi
    winmm
    mfplat
    mfreadwrite
    mfuuid
    propsys
)


add_custom_command(TARGET Storyland POST_BUILD
    # Storyland GLSL is compiled directly from src/shaders/storyland_shaders.cpp.
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "$<TARGET_FILE_DIR:Storyland>/timecyc.dat"
        "$<TARGET_FILE_DIR:Storyland>/zlib1.dll"
        "$<TARGET_FILE_DIR:Storyland>/zlibd1.dll"
    COMMENT "Removing obsolete timecycle and zlib DLL artifacts"
)
