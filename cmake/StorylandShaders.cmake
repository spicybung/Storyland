file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/shader_sources.json" STORYLAND_SHADER_CATALOG)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/shader_sources.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/storyland_shader_sources.h.in")

function(storyland_shader_path output_name source_name)
    string(JSON relative_path ERROR_VARIABLE lookup_error
        GET "${STORYLAND_SHADER_CATALOG}" sources "${source_name}")
    if(NOT lookup_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Unknown Storyland shader source: ${source_name}")
    endif()
    set(source_path "${CMAKE_CURRENT_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${source_path}")
        message(FATAL_ERROR "Missing Storyland shader source: ${relative_path}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source_path}")
    set(${output_name} "${source_path}" PARENT_SCOPE)
endfunction()

function(storyland_read_shader output_name source_name)
    storyland_shader_path(source_path "${source_name}")
    file(READ "${source_path}" shader_text)
    set(${output_name} "${shader_text}" PARENT_SCOPE)
endfunction()

foreach(game IN ITEMS lcs vcs)
    string(TOUPPER "${game}" game_upper)
    foreach(kind IN ITEMS world vehicle pedestrian vegetation generic)
        string(TOUPPER "${kind}" kind_upper)
        
        storyland_read_shader(STORYLAND_${game_upper}_${kind_upper}_HLSL "${game}_${kind}.hlsl")
        storyland_read_shader(STORYLAND_${game_upper}_${kind_upper}_VERTEX "${game}_${kind}.vert.glsl")
        storyland_read_shader(STORYLAND_${game_upper}_${kind_upper}_FRAGMENT "${game}_${kind}.frag.glsl")
    endforeach()
endforeach()
foreach(stage IN ITEMS vertex fragment)
    set(shader_path "vcs_psp_${stage}.glsl")
    
    storyland_read_shader(STORYLAND_VCS_PSP_${stage} "${shader_path}")
    set(ps2_shader_path "ps2_${stage}.glsl")
    
    storyland_read_shader(STORYLAND_PS2_${stage} "${ps2_shader_path}")
endforeach()
foreach(profile IN ITEMS lcs_ps2 lcs_psp vcs_ps2 vcs_psp)
    foreach(kind IN ITEMS world vehicle pedestrian vegetation generic)
        foreach(stage IN ITEMS vert frag)
            set(shader_path "${profile}_${kind}.${stage}.glsl")
            
            storyland_read_shader(STORYLAND_${profile}_${kind}_${stage} "${shader_path}")
            if(profile STREQUAL "vcs_psp")
                string(REPLACE "// @vcs_psp_vertex@" "${STORYLAND_VCS_PSP_vertex}"
                    STORYLAND_${profile}_${kind}_${stage} "${STORYLAND_${profile}_${kind}_${stage}}")
                string(REPLACE "// @vcs_psp_fragment@" "${STORYLAND_VCS_PSP_fragment}"
                    STORYLAND_${profile}_${kind}_${stage} "${STORYLAND_${profile}_${kind}_${stage}}")
            endif()
            if(profile STREQUAL "lcs_ps2" OR profile STREQUAL "vcs_ps2")
                string(REPLACE "// @ps2_vertex@" "${STORYLAND_PS2_vertex}"
                    STORYLAND_${profile}_${kind}_${stage} "${STORYLAND_${profile}_${kind}_${stage}}")
                string(REPLACE "// @ps2_fragment@" "${STORYLAND_PS2_fragment}"
                    STORYLAND_${profile}_${kind}_${stage} "${STORYLAND_${profile}_${kind}_${stage}}")
            endif()
        endforeach()
    endforeach()
endforeach()

storyland_read_shader(STORYLAND_CTW_VERTEX "ctw_preview.vert.glsl")
storyland_read_shader(STORYLAND_CTW_FRAGMENT "ctw_preview.frag.glsl")
foreach(stage IN ITEMS vert frag)
    
    storyland_read_shader(STORYLAND_AERO_${stage} "aero.${stage}.glsl")
endforeach()
storyland_read_shader(STORYLAND_NEO_VERTEX "skygfx.vert.glsl")
storyland_read_shader(STORYLAND_NEO_WORLD "skygfx.frag.glsl")
set(STORYLAND_NEO_VEHICLE "${STORYLAND_NEO_WORLD}")
string(REPLACE "#define STORYLAND_STYLE 0" "#define STORYLAND_STYLE 1" STORYLAND_CEL_FRAGMENT "${STORYLAND_NEO_WORLD}")
string(REPLACE "#define STORYLAND_STYLE 0" "#define STORYLAND_STYLE 2" STORYLAND_BUDOKAI_FRAGMENT "${STORYLAND_NEO_WORLD}")

foreach(stage IN ITEMS vert frag)
 
 storyland_read_shader(STORYLAND_DEFERRED_${stage} "aero_deferred.${stage}.glsl")
endforeach()

storyland_read_shader(STORYLAND_BLOOM_FRAGMENT "aero_bloom.frag.glsl")
# OpenGL 2.1 has no shader includes. Put the lamp code into both fragments here.

storyland_read_shader(STORYLAND_AERO_LIGHT "aero_light.glsl")
string(REPLACE "// @aero_light@" "${STORYLAND_AERO_LIGHT}" STORYLAND_AERO_frag "${STORYLAND_AERO_frag}")
string(REPLACE "// @aero_light@" "${STORYLAND_AERO_LIGHT}" STORYLAND_DEFERRED_frag "${STORYLAND_DEFERRED_frag}")
foreach(mobile_stage IN ITEMS mobile_world_vertex mobile_vehicle_vertex console_world_vertex mobile_world_fragment mobile_vehicle_fragment)
 
 storyland_read_shader(STORYLAND_${mobile_stage} "${mobile_stage}.glsl")
endforeach()

storyland_read_shader(STORYLAND_SAN_ANDREAS_FRAG "san_andreas.frag.glsl")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/storyland_shader_sources.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/storyland_shader_sources.h" @ONLY)
