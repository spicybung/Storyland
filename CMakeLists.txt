cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(Storyland VERSION 1.0.3 LANGUAGES CXX RC)

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
    src/storyland_archive.cpp
    src/storyland_anim.cpp
    src/storyland_wbl.cpp
    src/storyland_sky.cpp
    src/storyland_atomic_io.h
    src/storyland_dma_validator.cpp
    src/storyland_dma_validator.h
    src/storyland_psp_validator.cpp
    src/storyland_psp_validator.h
    src/storyland_model_validator.cpp
    src/storyland_model_validator.h
    "${STORYLAND_RESOURCE_FILE}"
)

target_include_directories(Storyland PRIVATE src)
target_link_libraries(Storyland PRIVATE ZLIB::ZLIB windowscodecs ole32 comctl32 comdlg32 shell32 advapi32 opengl32 gdi32 dwmapi)


add_custom_command(TARGET Storyland POST_BUILD
    # The active Stories GLSL is compiled into main.cpp. Remove artifacts left
    # by older builds so the Release folder accurately shows its dependencies.
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "$<TARGET_FILE_DIR:Storyland>/shaders"
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "$<TARGET_FILE_DIR:Storyland>/timecyc.dat"
        "$<TARGET_FILE_DIR:Storyland>/zlib1.dll"
        "$<TARGET_FILE_DIR:Storyland>/zlibd1.dll"
    COMMENT "Removing obsolete external shaders, timecycle, and zlib DLLs"
)
