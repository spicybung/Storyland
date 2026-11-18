#include "src/shaders/storyland_shaders.h"
#include "storyland_shader_sources.h"
// World, skin and vehicle passes keep their own shader pairs.
const char* storylandStoriesVertexShaderSource(bool vcs,StorylandShaderKind kind) {
    switch(kind) {
    case StorylandShaderKind::World:return vcs?StorylandShaderSources::vcs_world_vertex:StorylandShaderSources::lcs_world_vertex;
    case StorylandShaderKind::Vehicle:return vcs?StorylandShaderSources::vcs_vehicle_vertex:StorylandShaderSources::lcs_vehicle_vertex;
    case StorylandShaderKind::Pedestrian:return vcs?StorylandShaderSources::vcs_pedestrian_vertex:StorylandShaderSources::lcs_pedestrian_vertex;
    case StorylandShaderKind::Vegetation:return vcs?StorylandShaderSources::vcs_vegetation_vertex:StorylandShaderSources::lcs_vegetation_vertex;
    case StorylandShaderKind::Generic:return vcs?StorylandShaderSources::vcs_generic_vertex:StorylandShaderSources::lcs_generic_vertex;
    }
    return nullptr;
}
const char* storylandStoriesFragmentShaderSource(bool vcs,StorylandShaderKind kind) {
    switch(kind) {
    case StorylandShaderKind::World:return vcs?StorylandShaderSources::vcs_world_fragment:StorylandShaderSources::lcs_world_fragment;
    case StorylandShaderKind::Vehicle:return vcs?StorylandShaderSources::vcs_vehicle_fragment:StorylandShaderSources::lcs_vehicle_fragment;
    case StorylandShaderKind::Pedestrian:return vcs?StorylandShaderSources::vcs_pedestrian_fragment:StorylandShaderSources::lcs_pedestrian_fragment;
    case StorylandShaderKind::Vegetation:return vcs?StorylandShaderSources::vcs_vegetation_fragment:StorylandShaderSources::lcs_vegetation_fragment;
    case StorylandShaderKind::Generic:return vcs?StorylandShaderSources::vcs_generic_fragment:StorylandShaderSources::lcs_generic_fragment;
    }
    return nullptr;
}
const char* storylandStoriesHlslSource(bool vcs,StorylandShaderKind kind) {
    switch(kind) {
    case StorylandShaderKind::World:return vcs?StorylandShaderSources::vcs_world_hlsl:StorylandShaderSources::lcs_world_hlsl;
    case StorylandShaderKind::Vehicle:return vcs?StorylandShaderSources::vcs_vehicle_hlsl:StorylandShaderSources::lcs_vehicle_hlsl;
    case StorylandShaderKind::Pedestrian:return vcs?StorylandShaderSources::vcs_pedestrian_hlsl:StorylandShaderSources::lcs_pedestrian_hlsl;
    case StorylandShaderKind::Vegetation:return vcs?StorylandShaderSources::vcs_vegetation_hlsl:StorylandShaderSources::lcs_vegetation_hlsl;
    case StorylandShaderKind::Generic:return vcs?StorylandShaderSources::vcs_generic_hlsl:StorylandShaderSources::lcs_generic_hlsl;
    }
    return nullptr;
}
