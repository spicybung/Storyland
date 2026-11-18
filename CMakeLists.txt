cmake_minimum_required(VERSION 3.20)

include(cmake/StorylandVersionFile.cmake)
include(cmake/StorylandReleaseVersion.cmake)

# The Windows launcher explicitly configures CMake before every build.
# Do not let MSBuild trigger further, overlapping manifest installations
# while checking its generated project files.
if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    set(CMAKE_SUPPRESS_REGENERATION ON CACHE BOOL
        "Use the Storyland build launcher for CMake regeneration" FORCE)
endif()
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(Storyland VERSION ${STORYLAND_APPLICATION_VERSION} LANGUAGES CXX)

include(cmake/StorylandDependencies.cmake)
storyland_stage_dependency(lua STORYLAND_LUA_ROOT)
storyland_stage_dependency(opensubdiv STORYLAND_OPENSUBDIV_ROOT)
if(ANDROID)
    storyland_stage_dependency(sdl2 STORYLAND_SDL2_ROOT)
    storyland_stage_dependency(ffmpeg-source STORYLAND_FFMPEG_SOURCE_ROOT)
    storyland_stage_dependency(ffmpeg-android STORYLAND_FFMPEG_ANDROID_ROOT)
elseif(WIN32 OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64"))
    storyland_stage_dependency(espeak-ng STORYLAND_ESPEAK_ROOT)
endif()

# Set the runtime before creating any target, including OpenSubdiv.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set_source_files_properties(vendors/mikktspace/mikktspace.c PROPERTIES LANGUAGE CXX)

# Lua's supported C++ build uses exceptions, so invalid mod manifests unwind
# host strings/vectors normally instead of jumping over their destructors.
file(GLOB STORYLAND_LUA_SOURCES CONFIGURE_DEPENDS "${STORYLAND_LUA_ROOT}/src/*.c")
list(FILTER STORYLAND_LUA_SOURCES EXCLUDE REGEX "/(lua|luac)\\.c$")
set_source_files_properties(${STORYLAND_LUA_SOURCES} PROPERTIES LANGUAGE CXX)
add_library(storyland_lua STATIC ${STORYLAND_LUA_SOURCES})
set_target_properties(storyland_lua PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(storyland_lua PUBLIC "${STORYLAND_LUA_ROOT}/src")

# Pixar OpenSubdiv 3.6.1: Far CPU topology/refinement only; no runtime dependency.
file(GLOB STORYLAND_SUBDIV_SOURCES CONFIGURE_DEPENDS
    "${STORYLAND_OPENSUBDIV_ROOT}/opensubdiv/far/*.cpp"
    "${STORYLAND_OPENSUBDIV_ROOT}/opensubdiv/sdc/*.cpp"
    "${STORYLAND_OPENSUBDIV_ROOT}/opensubdiv/vtr/*.cpp")
add_library(storyland_subdiv STATIC ${STORYLAND_SUBDIV_SOURCES})
set_property(TARGET storyland_subdiv PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_include_directories(storyland_subdiv PUBLIC "${STORYLAND_OPENSUBDIV_ROOT}")
target_compile_definitions(storyland_subdiv PUBLIC _USE_MATH_DEFINES)

option(STORYLAND_ENABLE_DIRECTX "Build the optional native Direct3D 9 renderer" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(storyland_subdivision_bridge STATIC src/graphics/storyland_subdivision.cpp)
target_include_directories(storyland_subdivision_bridge PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" src/leeds)
target_link_libraries(storyland_subdivision_bridge PUBLIC storyland_subdiv)

include(cmake/StorylandShaders.cmake)

if(WIN32)
    enable_language(RC)
endif()

if(MSVC)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
        "$<$<COMPILE_LANGUAGE:CXX>:/bigobj>"
    )
endif()

if(MINGW)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wa,-mbig-obj>")
    add_link_options(-municode -static -static-libgcc -static-libstdc++)
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
if(WIN32)
    set(ZLIB_USE_STATIC_LIBS ON)
endif()
set(STORYLAND_PORTABLE_SOURCES
    src/storyland/storyland_movie_player.cpp
    src/audio/storyland_mission_voice_profiles.cpp
    src/audio/storyland_mission_voice_backend.cpp
    src/audio/storyland_mission_voice_neural_backend.cpp
    src/audio/storyland_mission_voice_preview.cpp
    src/storyland/storyland_loading_screens.cpp
    src/storyland/storyland_pc_assets.cpp
    src/storyland/storyland_pc_hud.cpp
    src/platform/linux/storyland_portable_coop.cpp
    src/platform/linux/storyland_linux_audio.cpp
    src/platform/linux/storyland_linux_renderer.cpp
    src/platform/linux/storyland_linux_session.cpp
    src/storyland/storyland_scm.cpp
    src/storyland/storyland_scm_workbench.cpp
    src/storyland/storyland_modding_ui.cpp
    src/storyland/storyland_dyom.cpp
    src/storyland/storyland_data.cpp
    src/storyland/storyland_gxt.cpp
    src/storyland/storyland_sky.cpp
    src/graphics/storyland_native_graphics.cpp
    src/shaders/storyland_shaders.cpp
    vendors/mikktspace/mikktspace.c
    src/leeds/leeds_texture.cpp
    src/leeds/leeds_gim.cpp
    src/leeds/leeds_archive.cpp
    src/leeds/leeds_native_scenery.cpp
    src/leeds/leeds_img_stream.cpp
    src/storyland/storyland_player_reactions_runtime.cpp
    src/storyland/storyland_player_reactions.cpp
    src/storyland/storyland_player_restart.cpp
    src/storyland/storyland_police.cpp
    src/storyland/storyland_play_test.cpp
    src/storyland/storyland_play_test_camera.cpp
    src/storyland/storyland_play_test_hud.cpp
    src/storyland/storyland_play_test_runtime.cpp
    src/storyland/storyland_extended_movement.cpp
    src/storyland/storyland_screen_droplets.cpp
    src/re/reVCS/gangs/storyland_vcs_gang_turfs.cpp
    src/re/reVCS/gangs/storyland_vcs_gang_turfs_live.cpp
    src/re/reVCS/pedestrians/storyland_vcs_ped_motion_runtime.cpp
    src/storyland/storyland_play_rubbish.cpp
    src/storyland/storyland_play_test_weapons.cpp
    src/storyland/storyland_blood_effects.cpp
    src/storyland/storyland_bullet_reaction.cpp
    src/storyland/storyland_vehicle_ped_impact.cpp
    src/storyland/storyland_play_test_script.cpp
    src/leeds/leeds_native_texture.cpp
    src/storyland/storyland_mission_vm.cpp
    src/re/reLCS2/script/CoreCommands.inl
    src/storyland/storyland_script_world_query.h
    src/storyland/storyland_mission_program.cpp
    src/storyland/storyland_cutscene.cpp
    src/leeds/leeds_native_world.cpp
    src/leeds/leeds_dtz.cpp
    src/re/reVCS/modloader/storyland_vcs_modloader.cpp
    src/re/reVCS/modloader/storyland_vcs_modloader_install.cpp
    src/leeds/leeds_model.cpp
    src/leeds/leeds_dff_conversion.cpp
    src/leeds/leeds_anim.cpp
    src/storyland/storyland_mobile_animation.cpp
    src/leeds/leeds_wbl.cpp
    src/leeds/leeds_ctw_pak.cpp
    src/leeds/leeds_ctw_city.cpp
    src/validation/storyland_dma_validator.cpp
    src/validation/storyland_psp_validator.cpp
    src/validation/storyland_model_validator.cpp
)

if(ANDROID)
    set_target_properties(storyland_subdiv storyland_subdivision_bridge PROPERTIES POSITION_INDEPENDENT_CODE ON)
    include(android/native.cmake)
    return()
endif()

add_subdirectory(vendors)

if(NOT WIN32)
    find_package(SDL2 REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(PNG REQUIRED)
    find_package(Threads REQUIRED)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(STORYLAND_AUDIO REQUIRED IMPORTED_TARGET libavformat>=59 libavcodec>=59 libavutil>=57 libswscale>=6)
    add_executable(Storyland src/platform/linux/main.cpp
        src/platform/linux/storyland_linux_ctw_viewer.cpp ${STORYLAND_PORTABLE_SOURCES})
    target_include_directories(Storyland PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        src/audio
        src/platform/linux src/storyland src/leeds src/graphics src/shaders src/validation src/gameplay
    )
    target_compile_definitions(Storyland PRIVATE STORYLAND_VERSION="${PROJECT_VERSION}" STORYLAND_HAS_COMPRESSED_AUDIO)
    target_link_libraries(Storyland PRIVATE
        SDL2::SDL2 OpenGL::GL PNG::PNG Threads::Threads OpenSSL::Crypto PkgConfig::STORYLAND_AUDIO storyland::zlib imgui::imgui storyland_lua ${CMAKE_DL_LIBS}
    )
    add_custom_command(TARGET Storyland POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Storyland>/sannybuilder"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vendors/sannybuilder/LCSSCM.INI"
            "${CMAKE_CURRENT_SOURCE_DIR}/vendors/sannybuilder/VCSSCM.INI"
            "$<TARGET_FILE_DIR:Storyland>/sannybuilder"
    )
    add_custom_command(TARGET Storyland POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/animations"
            "$<TARGET_FILE_DIR:Storyland>/assets/animations"
        COMMENT "Copying converted animation banks"
        VERBATIM
    )
    target_link_libraries(Storyland PRIVATE storyland_subdivision_bridge)
    target_link_libraries(Storyland PRIVATE storyland_subdiv)
    include(cmake/StorylandSpeech.cmake)
    storyland_stage_speech_runtime(Storyland)
    include(cmake/StorylandLoadingArtwork.cmake)
    storyland_stage_loading_artwork(Storyland)
    include(cmake/StorylandPcArtwork.cmake)
    storyland_stage_pc_artwork(Storyland)
    include(cmake/StorylandMovieAssets.cmake)
    storyland_stage_movie_assets(Storyland)
    install(TARGETS Storyland RUNTIME DESTINATION bin)
    install(DIRECTORY assets/modloader DESTINATION bin/assets)
    install(DIRECTORY assets/animations DESTINATION bin/assets)
    add_custom_command(TARGET Storyland POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/radio" "$<TARGET_FILE_DIR:Storyland>/assets/radio"
        COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:Storyland>/assets/radio/usertracks.png"
        COMMENT "Copying native radio textures"
        VERBATIM
    )
    install(DIRECTORY assets/radio DESTINATION bin/assets)
    return()
endif()

set(STORYLAND_RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows/Storyland.rc")
set_source_files_properties(
    "${STORYLAND_RESOURCE_FILE}"
    PROPERTIES
        LANGUAGE RC
        VS_TOOL_OVERRIDE "ResourceCompile"
)

add_executable(Storyland WIN32
    src/audio/storyland_mission_voice_profiles.cpp
    src/audio/storyland_mission_voice_backend.cpp
    src/audio/storyland_mission_voice_neural_backend.cpp
    src/audio/storyland_mission_voice_preview.cpp
    src/storyland/storyland_loading_screens.cpp
    src/storyland/storyland_pc_assets.cpp
    src/storyland/storyland_pc_hud.cpp
    src/audio/storyland_mission_voice_profiles.h
    src/audio/storyland_mission_voice_data.inl
    src/storyland/main.cpp
    src/storyland/storyland_analysis_canvas.cpp
    src/storyland/storyland_analysis_canvas.h
    src/storyland/storyland_analysis_graph.cpp
    src/storyland/storyland_analysis_graph.h
    src/graphics/storyland_graphics_synthesizer.cpp
    src/graphics/storyland_graphics_synthesizer.h
    src/graphics/storyland_antialiasing.cpp
    src/graphics/storyland_antialiasing.h
    src/graphics/storyland_native_graphics.cpp
    src/graphics/storyland_native_graphics.h
    src/graphics/storyland_world_texture.h
    src/shaders/storyland_shaders.cpp
        vendors/mikktspace/mikktspace.c
    src/shaders/storyland_shaders.h
    src/storyland/storyland_sky.cpp
    src/storyland/storyland_sky.h
    src/storyland/storyland_scm.cpp
    src/storyland/storyland_scm_workbench.cpp
    src/storyland/storyland_scm_workbench.h
    src/storyland/storyland_modding_ui.cpp
    src/storyland/storyland_modding_ui.h
    src/storyland/storyland_dyom.cpp
    src/storyland/storyland_dyom.h
    src/storyland/storyland_data.cpp
    src/storyland/storyland_data.h
    src/storyland/storyland_gxt.cpp
    src/storyland/storyland_scm.h
    src/storyland/storyland_media.cpp
    src/storyland/storyland_manhunt_audio.cpp
    src/storyland/storyland_manhunt_ahx.cpp
    src/storyland/storyland_media.h
    src/storyland/storyland_atomic_io.h

    src/leeds/leeds_texture.cpp
    src/leeds/leeds_gim.cpp
    src/leeds/leeds_gim.h
    src/leeds/leeds_texture.h
    src/leeds/leeds_archive.cpp
    src/leeds/leeds_native_scenery.cpp
    src/leeds/leeds_img_stream.cpp
    src/storyland/storyland_player_reactions_runtime.cpp
    src/storyland/storyland_player_reactions.cpp
    src/storyland/storyland_player_restart.cpp
    src/storyland/storyland_police.cpp
    src/leeds/leeds_archive.h
    src/storyland/storyland_visibility.h
    src/storyland/storyland_play_test.cpp
    src/storyland/storyland_play_test.h
    src/storyland/storyland_play_test_camera.cpp
    src/storyland/storyland_play_test_camera.h
    src/storyland/storyland_play_test_runtime.cpp
    src/storyland/storyland_extended_movement.cpp
    src/storyland/storyland_screen_droplets.cpp
    src/re/reVCS/gangs/storyland_vcs_gang_turfs.cpp
    src/re/reVCS/gangs/storyland_vcs_gang_turfs_live.cpp
    src/re/reVCS/pedestrians/storyland_vcs_ped_motion_runtime.cpp
    src/storyland/storyland_play_rubbish.cpp
    src/storyland/storyland_play_test_weapons.cpp
    src/storyland/storyland_blood_effects.cpp
    src/storyland/storyland_bullet_reaction.cpp
    src/storyland/storyland_vehicle_ped_impact.cpp
    src/storyland/storyland_play_test_runtime.h
    src/storyland/storyland_debug_movement.h
    src/storyland/storyland_crime.h
    src/storyland/storyland_play_test_hud.cpp
    src/leeds/leeds_native_texture.cpp
    src/leeds/leeds_native_texture.h
    src/leeds/leeds_ps2_texture_layout.h
    src/storyland/storyland_play_test_hud.h
    src/storyland/storyland_play_test_script.cpp
    src/storyland/storyland_actor_draw_cache.h
    src/storyland/storyland_play_test_script.h
    src/storyland/storyland_mission_vm.cpp
    src/re/reLCS2/script/CoreCommands.inl
    src/storyland/storyland_script_world_query.h
    src/storyland/storyland_mission_vm.h
    src/storyland/storyland_cutscene.cpp
    src/storyland/storyland_cutscene.h
    src/storyland/storyland_mission_program.cpp
    src/storyland/storyland_mission_program.h
    src/re/reVCS/script/Commands.inl
    src/re/reLCS2/runtime/storyland_lcs_attachment.h
    src/re/reLCS2/world/storyland_lcs_dynamic_placement.h
    src/re/reLCS2/world/storyland_lcs_garages.h
    src/re/reLCS2/runtime/storyland_lcs_messages.h
    src/re/reLCS2/world/storyland_lcs_object_visibility.h
    src/re/reLCS2/world/storyland_lcs_static_placement.h
    src/re/reLCS2/pedestrians/storyland_lcs_ped_hit.h
    src/re/reLCS2/runtime/storyland_lcs_population.h
    src/re/reLCS2/vehicles/storyland_lcs_vehicle_access.h
    src/re/reLCS2/vehicles/storyland_lcs_vehicle_damage.h
    src/re/reLCS2/vehicles/storyland_lcs_vehicle_tasks.h
    src/re/reLCS2/vehicles/storyland_lcs_boat.h
    src/re/reLCS2/pedestrians/storyland_lcs_wait_animation.h
    src/re/reLCS2/pedestrians/storyland_lcs_wait_runtime.h
    src/re/reLCS2/world/storyland_lcs_named_texture.h
    src/storyland/storyland_script_camera.h
    src/storyland/storyland_script_corona.h
    src/storyland/storyland_script_vehicle_animation.h
    src/re/reLCS2/world/storyland_lcs_crane_render.h
    src/re/reLCS2/world/storyland_lcs_cranes.h
    src/re/reLCS2/camera/storyland_lcs_garage_camera.h
    src/re/reLCS2/vehicles/storyland_lcs_vehicle_checks.h
    src/storyland/storyland_vehicle_systems.h
    src/re/reLCS2/frontend/storyland_play_test_cranes.inc
    src/re/reLCS2/frontend/storyland_play_test_garage_camera.inc
    src/storyland/storyland_mission_radar.h
    src/leeds/leeds_native_world.cpp
    src/leeds/leeds_native_world.h
    src/leeds/leeds_dtz.cpp
    src/re/reVCS/modloader/storyland_vcs_modloader.cpp
    src/re/reVCS/modloader/storyland_vcs_modloader_install.cpp
    src/leeds/leeds_dtz.h
    src/leeds/leeds_model.cpp
    src/leeds/leeds_dff_conversion.cpp
    src/leeds/leeds_model.h
    src/leeds/leeds_model_materials.h
    src/leeds/leeds_anim.cpp
    src/storyland/storyland_mobile_animation.cpp
    src/storyland/storyland_mobile_animation.h
    src/leeds/leeds_anim.h
    src/leeds/leeds_ctw_pak.cpp
    src/leeds/leeds_ctw_pak.h
    src/leeds/leeds_ctw_city.cpp
    src/leeds/leeds_ctw_city.h
    src/graphics/storyland_ctw_city_view.h
    src/leeds/leeds_ctw_wad.cpp
    src/leeds/leeds_ctw_wad.h
    src/leeds/leeds_wbl.cpp
    src/leeds/leeds_wbl.h

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

if(WIN32 AND STORYLAND_ENABLE_DIRECTX)
    target_sources(Storyland PRIVATE src/graphics/storyland_directx.cpp src/graphics/storyland_directx.h)
    target_compile_definitions(Storyland PRIVATE STORYLAND_ENABLE_DIRECTX=1)
    target_compile_definitions(storyland_imgui PRIVATE STORYLAND_ENABLE_DIRECTX=1)
    target_link_libraries(Storyland PRIVATE d3d9 d3dcompiler)
endif()

target_include_directories(Storyland PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    src/audio
    src/shaders
    src/storyland
    src/graphics
    src/leeds
    src/gameplay
    src/validation
    src/platform/windows
)
set(STORYLAND_GAME_SHADER_FILES)
foreach(game IN ITEMS lcs vcs)
    foreach(kind IN ITEMS world vehicle pedestrian vegetation generic)
        foreach(extension IN ITEMS vert.glsl frag.glsl hlsl)
            storyland_shader_path(shader_file "${game}_${kind}.${extension}")
            list(APPEND STORYLAND_GAME_SHADER_FILES "${shader_file}")
        endforeach()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES STORYLAND_GAME_SHADER_FILES)
set_source_files_properties(${STORYLAND_GAME_SHADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(Storyland PRIVATE ${STORYLAND_GAME_SHADER_FILES})

target_compile_definitions(Storyland PRIVATE UNICODE _UNICODE)
include(cmake/StorylandSpeech.cmake)
storyland_stage_speech_runtime(Storyland)
include(cmake/StorylandLoadingArtwork.cmake)
storyland_stage_loading_artwork(Storyland)
include(cmake/StorylandPcArtwork.cmake)
storyland_stage_pc_artwork(Storyland)
include(cmake/StorylandMovieAssets.cmake)
storyland_stage_movie_assets(Storyland)
find_package(SDL2 CONFIG REQUIRED)
if(TARGET SDL2::SDL2-static)
    target_link_libraries(Storyland PRIVATE SDL2::SDL2-static)
else()
    target_link_libraries(Storyland PRIVATE SDL2::SDL2)
endif()

target_link_libraries(Storyland PRIVATE
    storyland_lua
    storyland::zlib
    imgui::imgui
    storyland::ffmpeg
    secur32
    ncrypt
    crypt32
    ws2_32
    bcrypt
    winhttp
    strmiids
    windowscodecs
    ole32
    user32
    comctl32
    comdlg32
    shell32
    uuid
    advapi32
    opengl32
    gdi32
    msimg32
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

add_custom_command(TARGET Storyland POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Storyland>/sannybuilder"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/vendors/sannybuilder/LCSSCM.INI"
        "$<TARGET_FILE_DIR:Storyland>/sannybuilder/LCSSCM.INI"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/vendors/sannybuilder/VCSSCM.INI"
        "$<TARGET_FILE_DIR:Storyland>/sannybuilder/VCSSCM.INI"
    COMMENT "Copying LCS/VCS opcode definitions"
)

# Embed the supplied music recordings; missing assets must fail the build.
foreach(STORYLAND_MUSIC IN ITEMS reigns_error_theme vcs_error_warning_theme
    gta3_error_theme gtavc_error_theme gtasa_error_theme storyland_about_credits manhunt2_crash_theme fallback_storyland_intro)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/${STORYLAND_MUSIC}.wav")
        message(FATAL_ERROR "Required music recording is missing: ${STORYLAND_MUSIC}.wav")
    endif()
endforeach()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/storyland_intro.wav")
    target_compile_definitions(Storyland PRIVATE STORYLAND_HAS_CUSTOM_INTRO_THEME=1)
endif()
foreach(STORYLAND_THEME IN ITEMS REIGNS_ERROR_THEME VCS_ERROR_WARNING_THEME GTA3_ERROR_THEME GTAVC_ERROR_THEME GTASA_ERROR_THEME)
    string(TOLOWER "${STORYLAND_THEME}" STORYLAND_THEME_FILE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/${STORYLAND_THEME_FILE}.wav")
        target_compile_definitions(Storyland PRIVATE "STORYLAND_HAS_${STORYLAND_THEME}=1")
    endif()
endforeach()

add_custom_command(TARGET Storyland POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Storyland>/assets/gui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/gui/vcs_vic_jetski.png"
        "$<TARGET_FILE_DIR:Storyland>/assets/gui/vcs_vic_jetski.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/gui/lcs_toni_bike.png"
        "$<TARGET_FILE_DIR:Storyland>/assets/gui/lcs_toni_bike.png"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/water" "$<TARGET_FILE_DIR:Storyland>/assets/water"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/radio" "$<TARGET_FILE_DIR:Storyland>/assets/radio"
    COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:Storyland>/assets/radio/usertracks.png"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/animations" "$<TARGET_FILE_DIR:Storyland>/assets/animations"
    COMMENT "Copying editor riders, water textures and radio artwork"
)

# Interface music is embedded by Storyland.rc; remove copies left by older builds.
foreach(STORYLAND_EMBEDDED_THEME IN ITEMS
    storyland_intro fallback_storyland_intro reigns_error_theme vcs_error_warning_theme gta3_error_theme
    gtavc_error_theme gtasa_error_theme storyland_about_credits manhunt2_crash_theme)
    add_custom_command(TARGET Storyland POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:Storyland>/${STORYLAND_EMBEDDED_THEME}.wav"
            "$<TARGET_FILE_DIR:Storyland>/${STORYLAND_EMBEDDED_THEME}.mp3"
        VERBATIM
    )
endforeach()


target_link_libraries(Storyland PRIVATE storyland_subdivision_bridge)
target_link_libraries(Storyland PRIVATE storyland_subdiv)
