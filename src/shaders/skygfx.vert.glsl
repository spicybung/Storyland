#version 120
uniform vec3 uAmbientColor;
uniform vec3 uDirectionalColor;
uniform vec3 uLightEye;
uniform float uFogStart;
uniform float uFarClip;
uniform int uNeoVehicle;
uniform int uPrelightMode;
varying vec4 vMaterialColor;
varying vec2 vBaseUv;
varying vec2 vDetailUv;
varying vec2 vEnvironmentUv;
varying vec3 vNormalEye;
varying vec3 vViewEye;
varying float vFogFactor;
varying float vReflectionFresnel;
void main() {
    vec4 eyePosition = gl_ModelViewMatrix * gl_Vertex;
    gl_Position = gl_ProjectionMatrix * eyePosition;
    vec3 normalEye = gl_NormalMatrix * gl_Normal;
    float normalLength = length(normalEye);
    vNormalEye = normalLength > 0.00001 ? normalEye / normalLength : vec3(0.0, 0.0, 1.0);
    vViewEye = normalize(-eyePosition.xyz + vec3(0.00001));
    vBaseUv = gl_MultiTexCoord0.xy;
    vDetailUv = gl_MultiTexCoord1.xy;
    vec3 reflected = reflect(-vViewEye, vNormalEye);
    vEnvironmentUv = reflected.xy * 0.5 + vec2(0.5);
    float edge = 1.0 - clamp(dot(vNormalEye, vViewEye), 0.0, 1.0);
    vReflectionFresnel = edge * edge * edge * edge * edge;
    vMaterialColor = gl_Color;
    vFogFactor = clamp((uFarClip - length(eyePosition.xyz)) / max(uFarClip - uFogStart, 0.001), 0.0, 1.0);
}
