function(storyland_stage_speech_runtime target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/licenses/storyland_fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vendors/storyland_fonts/LICENSE.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/storyland_fonts/LICENSE.txt"
        VERBATIM
    )
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/vendors/storyland_fonts/LICENSE.txt"
        DESTINATION bin/licenses/storyland_fonts)
    if(WIN32)
        set(runtime_directory "${STORYLAND_ESPEAK_ROOT}/windows")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(runtime_directory "${STORYLAND_ESPEAK_ROOT}/linux")
    else()
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/speech/neural"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/storyland_neural_voice_worker.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/setup_neural_voice.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/data/vcs_approved_voice_lines.json"
            "$<TARGET_FILE_DIR:${target}>/speech/neural"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/data/mission_voice_neural_config.example.json"
            "$<TARGET_FILE_DIR:${target}>/speech/neural/voice_config.example.json"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/vendors/chatterbox"
            "$<TARGET_FILE_DIR:${target}>/speech/neural/setup_resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/vendors/perth"
            "$<TARGET_FILE_DIR:${target}>/speech/neural/setup_resources/perth"
        COMMENT "Staging the reference-conditioned neural voice worker and setup resources"
    )
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/storyland_neural_voice_worker.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/setup_neural_voice.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/vcs_approved_voice_lines.json"
        DESTINATION bin/speech/neural)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/mission_voice_neural_config.example.json"
        DESTINATION bin/speech/neural RENAME voice_config.example.json)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vendors/chatterbox/"
        DESTINATION bin/speech/neural/setup_resources)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vendors/perth/"
        DESTINATION bin/speech/neural/setup_resources/perth)
    if(IS_DIRECTORY "${runtime_directory}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${runtime_directory}" "$<TARGET_FILE_DIR:${target}>/speech"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${STORYLAND_ESPEAK_ROOT}/licenses"
                "$<TARGET_FILE_DIR:${target}>/speech/licenses"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${STORYLAND_ESPEAK_ROOT}/runtime-provenance.json"
                "$<TARGET_FILE_DIR:${target}>/speech/runtime-provenance.json"
            COMMENT "Staging the explicit eSpeak diagnostic backend"
        )
        install(DIRECTORY "${runtime_directory}/" DESTINATION bin/speech)
        install(DIRECTORY "${STORYLAND_ESPEAK_ROOT}/licenses/"
            DESTINATION bin/speech/licenses)
    endif()
endfunction()

option(STORYLAND_BUILD_VOICE_TOOL "Build the approved dialogue WAV renderer" ON)
if(STORYLAND_BUILD_VOICE_TOOL AND NOT TARGET storyland-voice-render)
    add_executable(storyland-voice-render
        src/audio/storyland_mission_voice_render.cpp
        src/audio/storyland_mission_voice_profiles.cpp
        src/audio/storyland_mission_voice_backend.cpp
        src/audio/storyland_mission_voice_neural_backend.cpp
    )
    target_compile_features(storyland-voice-render PRIVATE cxx_std_17)
    target_include_directories(storyland-voice-render PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT WIN32)
        target_link_libraries(storyland-voice-render PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    endif()
    storyland_stage_speech_runtime(storyland-voice-render)
    install(TARGETS storyland-voice-render RUNTIME DESTINATION bin)
endif()
