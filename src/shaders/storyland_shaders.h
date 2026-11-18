#pragma once
enum class StorylandShaderKind { World, Vehicle, Pedestrian, Vegetation, Generic };
const char* storylandStoriesVertexShaderSource(bool vcs=false,StorylandShaderKind kind=StorylandShaderKind::Generic);
const char* storylandStoriesFragmentShaderSource(bool vcs=false,StorylandShaderKind kind=StorylandShaderKind::Generic);
const char* storylandStoriesHlslSource(bool vcs=false,StorylandShaderKind kind=StorylandShaderKind::Generic);
