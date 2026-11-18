#version 120
varying vec3 eyePosition;
varying vec2 textureCoordinate;
varying vec4 materialColour;

void main() {
    vec4 eye = gl_ModelViewMatrix * gl_Vertex;
    // Use the same space for the fragment normal and the sun direction.
    eyePosition = eye.xyz;
    textureCoordinate = gl_MultiTexCoord0.xy;
    materialColour = gl_Color;
    gl_Position = gl_ProjectionMatrix * eye;
}
