cmake_minimum_required(VERSION 3.20)
project(Storyland LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)

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
    "${STORYLAND_RESOURCE_FILE}"
)

target_include_directories(Storyland PRIVATE src)
target_link_libraries(Storyland PRIVATE ZLIB::ZLIB windowscodecs ole32 comctl32 comdlg32 shell32 opengl32 gdi32)
