# Windows FFmpeg is supplied by the configured static vcpkg triplet.
# Android source and libraries are in vendors/ffmpeg/source and vendors/ffmpeg/android.
# Separate imported targets preserve Release/Debug library selection.
find_path(STORYLAND_FFMPEG_INCLUDE_DIR NAMES libavcodec/avcodec.h REQUIRED)
get_filename_component(STORYLAND_FFMPEG_TRIPLET_ROOT "${STORYLAND_FFMPEG_INCLUDE_DIR}" DIRECTORY)

# Some FFmpeg builds enable decoders which also depend on libswresample.
# Keep this optional for existing vcpkg builds that do not include it.
set(STORYLAND_FFMPEG_COMPONENTS avformat avcodec swscale)
find_library(STORYLAND_SWRESAMPLE_RELEASE NAMES swresample
    PATHS "${STORYLAND_FFMPEG_TRIPLET_ROOT}/lib" NO_DEFAULT_PATH)
if(STORYLAND_SWRESAMPLE_RELEASE)
    list(APPEND STORYLAND_FFMPEG_COMPONENTS swresample)
endif()
list(APPEND STORYLAND_FFMPEG_COMPONENTS avutil)
set(STORYLAND_FFMPEG_TARGETS)
foreach(component IN LISTS STORYLAND_FFMPEG_COMPONENTS)
    string(TOUPPER "${component}" component_upper)
    find_library(STORYLAND_${component_upper}_RELEASE
        NAMES ${component}
        PATHS "${STORYLAND_FFMPEG_TRIPLET_ROOT}/lib"
        NO_DEFAULT_PATH REQUIRED)
    find_library(STORYLAND_${component_upper}_DEBUG
        NAMES ${component}
        PATHS "${STORYLAND_FFMPEG_TRIPLET_ROOT}/debug/lib"
        NO_DEFAULT_PATH)
    if(NOT STORYLAND_${component_upper}_DEBUG)
        set(STORYLAND_${component_upper}_DEBUG "${STORYLAND_${component_upper}_RELEASE}")
    endif()
    add_library(storyland_ffmpeg_${component} UNKNOWN IMPORTED GLOBAL)
    set_target_properties(storyland_ffmpeg_${component} PROPERTIES
        IMPORTED_CONFIGURATIONS "RELEASE;DEBUG"
        IMPORTED_LOCATION_RELEASE "${STORYLAND_${component_upper}_RELEASE}"
        IMPORTED_LOCATION_DEBUG "${STORYLAND_${component_upper}_DEBUG}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release
        INTERFACE_INCLUDE_DIRECTORIES "${STORYLAND_FFMPEG_INCLUDE_DIR}")
    list(APPEND STORYLAND_FFMPEG_TARGETS storyland_ffmpeg_${component})
endforeach()

add_library(storyland_ffmpeg INTERFACE)
add_library(storyland::ffmpeg ALIAS storyland_ffmpeg)
target_link_libraries(storyland_ffmpeg INTERFACE ${STORYLAND_FFMPEG_TARGETS}
    ZLIB::ZLIB bcrypt ole32 user32)
if(MINGW)
    target_link_libraries(storyland_ffmpeg INTERFACE atomic m)
endif()
