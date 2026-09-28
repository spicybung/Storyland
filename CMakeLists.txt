cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
if(WIN32)
    project(Storyland VERSION 1.1.5.2 LANGUAGES CXX RC)
else()
    project(Storyland VERSION 1.1.5.2 LANGUAGES CXX)
endif()

if(UNIX AND NOT APPLE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(ZLIB REQUIRED)
    add_executable(StorylandLinux
        src/storyland_linux.cpp
        src/storyland_dtz.cpp
        src/storyland_archive.cpp
        src/storyland_dma_validator.cpp
    )
    target_include_directories(StorylandLinux PRIVATE src)
    target_link_libraries(StorylandLinux PRIVATE ZLIB::ZLIB)
    return()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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
find_library(STORYLAND_AVCODEC_LIBRARY NAMES avcodec REQUIRED)
find_library(STORYLAND_AVFORMAT_LIBRARY NAMES avformat REQUIRED)
find_library(STORYLAND_AVUTIL_LIBRARY NAMES avutil REQUIRED)
find_library(STORYLAND_SWSCALE_LIBRARY NAMES swscale REQUIRED)

set(STORYLAND_FFMPEG_LIBRARIES
    ${STORYLAND_AVFORMAT_LIBRARY}
    ${STORYLAND_AVCODEC_LIBRARY}
    ${STORYLAND_SWSCALE_LIBRARY}
    ${STORYLAND_AVUTIL_LIBRARY}
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

set(STORYLAND_RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/Storyland.rc")
set_source_files_properties(
    "${STORYLAND_RESOURCE_FILE}"
    PROPERTIES
        LANGUAGE RC
        VS_TOOL_OVERRIDE "ResourceCompile"
)

add_executable(Storyland WIN32
    src/main.cpp
    src/leeds_texture.cpp
    src/wic_image.cpp
    src/storyland_dtz.cpp
    src/storyland_model.cpp
    src/storyland_analysis_graph.cpp
    src/storyland_analysis_graph.h
    src/storyland_opengl_renderer.cpp
    src/storyland_opengl_renderer.h
    src/storyland_shaders.cpp
    src/storyland_shaders.h
    src/storyland_archive.cpp
    src/storyland_anim.cpp
    src/storyland_wbl.cpp
    src/storyland_sky.cpp
    src/storyland_scm.cpp
    src/storyland_scm.h
    src/storyland_atomic_io.h
    src/storyland_dma_validator.cpp
    src/storyland_dma_validator.h
    src/storyland_psp_validator.cpp
    src/storyland_psp_validator.h
    src/storyland_model_validator.cpp
    src/storyland_model_validator.h
    src/storyland_media.cpp
    src/storyland_media.h
    src/storyland_scripting.cpp
    src/storyland_scripting.h
    "${STORYLAND_RESOURCE_FILE}"
)

target_include_directories(Storyland PRIVATE src ${STORYLAND_FFMPEG_INCLUDE_DIR})
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
    # Storyland GLSL is compiled directly from src/storyland_shaders.cpp.
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "$<TARGET_FILE_DIR:Storyland>/timecyc.dat"
        "$<TARGET_FILE_DIR:Storyland>/zlib1.dll"
        "$<TARGET_FILE_DIR:Storyland>/zlibd1.dll"
    COMMENT "Removing obsolete timecycle and zlib DLL artifacts"
)
