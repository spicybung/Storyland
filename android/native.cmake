set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${STORYLAND_SDL2_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/vendors/SDL2")
add_library(storyland_imgui STATIC
    vendors/imgui/imgui.cpp vendors/imgui/imgui_draw.cpp
    vendors/imgui/imgui_tables.cpp vendors/imgui/imgui_widgets.cpp
    vendors/imgui/backends/imgui_impl_sdl2.cpp
    vendors/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(storyland_imgui PUBLIC vendors vendors/imgui vendors/imgui/backends)
target_compile_definitions(storyland_imgui PUBLIC IMGUI_USER_CONFIG="storyland_imgui_config.h" IMGUI_IMPL_OPENGL_ES3)
target_link_libraries(storyland_imgui PUBLIC SDL2::SDL2 GLESv3)
add_library(main SHARED
    src/platform/android/main.cpp src/platform/android/storyland_android_gl.cpp
    ${STORYLAND_PORTABLE_SOURCES})
target_include_directories(main PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    src/audio src/platform/android src/platform/linux src/storyland src/leeds src/graphics src/shaders src/validation)
target_compile_definitions(main PRIVATE STORYLAND_VERSION="${PROJECT_VERSION}" STORYLAND_USE_AV_HMAC STORYLAND_HAS_COMPRESSED_AUDIO)
target_link_libraries(main PRIVATE storyland_imgui storyland_subdivision_bridge storyland_lua SDL2::SDL2 GLESv3 android log z ${CMAKE_DL_LIBS})
target_link_options(main PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,-z,common-page-size=16384")
target_link_options(SDL2 PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,-z,common-page-size=16384")
if(NOT DEFINED STORYLAND_ANDROID_FFMPEG)
    set(STORYLAND_ANDROID_FFMPEG "${STORYLAND_FFMPEG_ANDROID_ROOT}/${ANDROID_ABI}")
endif()
foreach(component avformat avcodec swscale avutil)
    if(NOT EXISTS "${STORYLAND_ANDROID_FFMPEG}/lib/lib${component}.a")
        message(FATAL_ERROR
            "Android FFmpeg is missing for ${ANDROID_ABI}. Use android/build_apk.py, or build with "
            "android/build_ffmpeg.py and configure -DSTORYLAND_ANDROID_FFMPEG=<its installation directory>.")
    endif()
    add_library(storyland_android_${component} STATIC IMPORTED)
    set_target_properties(storyland_android_${component} PROPERTIES
        IMPORTED_LOCATION "${STORYLAND_ANDROID_FFMPEG}/lib/lib${component}.a")
endforeach()
target_include_directories(main PRIVATE "${STORYLAND_ANDROID_FFMPEG}/include")
target_sources(main PRIVATE src/platform/android/storyland_android_movie.cpp)
target_link_libraries(main PRIVATE storyland_android_avformat storyland_android_avcodec
    storyland_android_swscale storyland_android_avutil)
