include_guard(GLOBAL)

function(storyland_stage_loading_artwork target)
    set(artwork_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/loading/vcs")
    set(artwork_files)
    foreach(index RANGE 0 11)
        list(APPEND artwork_files "${artwork_directory}/loadsc${index}.xtx")
    endforeach()
    foreach(name IN ITEMS outro title_pc_EN title_pc_ES title_pc_PL)
        list(APPEND artwork_files "${artwork_directory}/${name}.xtx")
    endforeach()
    foreach(artwork IN LISTS artwork_files)
        if(NOT EXISTS "${artwork}" OR IS_DIRECTORY "${artwork}")
            message(FATAL_ERROR "Required VCS loading artwork is missing: ${artwork}")
        endif()
    endforeach()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${target}>/assets/loading/vcs"
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:${target}>/assets/loading/vcs/intro.xtx"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${artwork_files}
            "$<TARGET_FILE_DIR:${target}>/assets/loading/vcs"
        COMMENT "Copying VCS loading artwork"
        VERBATIM
    )
    install(FILES ${artwork_files} DESTINATION bin/assets/loading/vcs)
endfunction()
