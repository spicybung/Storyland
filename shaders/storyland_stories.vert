#version 120

varying vec4 vColor;
varying vec2 vTex0;
varying vec3 vLighting;
varying float vSpecular;
varying float vFog;

uniform vec3 uAmbientColor;
uniform vec3 uDirectionalColor;
uniform vec3 uSunDirection;
uniform float uFogStart;
uniform float uFarClip;

void main(){
  vec4 eyePos = gl_ModelViewMatrix * gl_Vertex;
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  vColor = gl_Color;
  vTex0 = gl_MultiTexCoord0.xy;

  vec3 n = normalize(gl_NormalMatrix * gl_Normal);
  vec3 lightDir = normalize(gl_NormalMatrix * uSunDirection);
  float direct = max(dot(n, lightDir), 0.0);
  float bounce = max(dot(n, -lightDir), 0.0) * 0.22;
  vLighting = clamp(uAmbientColor + uDirectionalColor * (direct + bounce), 0.0, 1.35);

  vec3 viewDir = normalize(-eyePos.xyz);
  vSpecular = pow(max(dot(reflect(-lightDir, n), viewDir), 0.0), 18.0) * 0.12;

  float distanceFromCamera = length(eyePos.xyz);
  float fogRange = max(uFarClip - uFogStart, 0.001);
  vFog = clamp((uFarClip - distanceFromCamera) / fogRange, 0.0, 1.0);
}
