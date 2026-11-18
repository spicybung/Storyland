# Dear ImGui v1.91.9-docking, upstream commit 126d004f9e1eef062bf4b044b3b2faaf58d48c51.
# Upstream source and its MIT license are kept together under imgui/.
find_package(ZLIB REQUIRED)
if(WIN32 AND DEFINED ZLIB_LIBRARY_RELEASE)
    file(TO_CMAKE_PATH "${ZLIB_LIBRARY_RELEASE}" STORYLAND_ZLIB_RELEASE_PATH)
    if(STORYLAND_ZLIB_RELEASE_PATH MATCHES "/x64-windows/lib/")
        message(FATAL_ERROR "Static zlib is required. Use the x64-windows-static triplet.")
    endif()
endif()
add_library(storyland_zlib INTERFACE)
add_library(storyland::zlib ALIAS storyland_zlib)
target_link_libraries(storyland_zlib INTERFACE ZLIB::ZLIB)
add_library(storyland_imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_opengl2.cpp
)
target_include_directories(storyland_imgui PRIVATE "${PROJECT_SOURCE_DIR}")
add_library(imgui::imgui ALIAS storyland_imgui)
target_include_directories(storyland_imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} imgui imgui/backends)
target_compile_definitions(storyland_imgui PUBLIC IMGUI_USER_CONFIG="storyland_imgui_config.h")
if(WIN32)
    target_sources(storyland_imgui PRIVATE imgui/backends/imgui_impl_win32.cpp)
    target_link_libraries(storyland_imgui PUBLIC opengl32 user32 gdi32 dwmapi shell32 imm32)
    add_subdirectory(ffmpeg)
else()
    find_package(SDL2 REQUIRED)
    find_package(OpenGL REQUIRED)
    target_sources(storyland_imgui PRIVATE imgui/backends/imgui_impl_sdl2.cpp)
    target_link_libraries(storyland_imgui PUBLIC SDL2::SDL2 OpenGL::GL)
endif()
