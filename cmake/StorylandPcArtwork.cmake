include_guard(GLOBAL)

function(storyland_stage_pc_artwork target)
    set(artwork_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/pc")
    set(artwork_manifest "${artwork_directory}/manifest.txt")
    if(NOT EXISTS "${artwork_manifest}")
        message(FATAL_ERROR "Required PC artwork manifest is missing: ${artwork_manifest}")
    endif()
    file(STRINGS "${artwork_manifest}" artwork_files)
    if(NOT artwork_files)
        message(FATAL_ERROR "The PC artwork manifest is empty.")
    endif()
    foreach(artwork IN LISTS artwork_files)
        if(NOT artwork MATCHES "^[A-Za-z0-9_./-]+$" OR
            NOT artwork MATCHES "\\.xtx$" OR
            artwork MATCHES "(^|/)\\.\\.(/|$)" OR IS_ABSOLUTE "${artwork}" OR
            NOT EXISTS "${artwork_directory}/${artwork}" OR
            IS_DIRECTORY "${artwork_directory}/${artwork}")
            message(FATAL_ERROR "Required PC artwork is missing or invalid: ${artwork}")
        endif()
    endforeach()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${artwork_directory}" "$<TARGET_FILE_DIR:${target}>/assets/pc"
        COMMENT "Copying PC presentation artwork"
        VERBATIM
    )
    install(DIRECTORY "${artwork_directory}" DESTINATION bin/assets)
endfunction()
