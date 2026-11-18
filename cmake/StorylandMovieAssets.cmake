include_guard(GLOBAL)

function(storyland_stage_movie_assets target)
    set(movie_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/movies")
    set(movie_files)
    foreach(name IN ITEMS intro_pc.pss reigns_studio.pss manifest.json)
        set(movie "${movie_directory}/${name}")
        if(NOT EXISTS "${movie}" OR IS_DIRECTORY "${movie}")
            message(FATAL_ERROR "Required Storyland movie asset is missing: ${movie}")
        endif()
        list(APPEND movie_files "${movie}")
    endforeach()
    if(EXISTS "${movie_directory}/README.md" AND NOT IS_DIRECTORY "${movie_directory}/README.md")
        list(APPEND movie_files "${movie_directory}/README.md")
    endif()

    add_custom_target(${target}_movie_assets
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${target}>/assets/movies"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${movie_files}
            "$<TARGET_FILE_DIR:${target}>/assets/movies"
        DEPENDS ${movie_files}
        COMMENT "Staging Storyland opening movies"
        VERBATIM
    )
    add_dependencies(${target} ${target}_movie_assets)
    install(FILES ${movie_files} DESTINATION bin/assets/movies)
endfunction()
