#version 120
varying vec2 uv;
// The resolve quad is already in clip space.
void main(){gl_Position=gl_Vertex;uv=gl_MultiTexCoord0.xy;}
