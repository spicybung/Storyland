#version 120
varying vec2 vNativeEnvironmentUV;
varying vec3 vNativeEnvironmentDirection;
uniform vec3 uEnvironmentViewRight;
uniform vec3 uEnvironmentViewUp;
uniform vec3 uEnvironmentViewBack;
uniform float uEnvironmentFaceSize;
vec2 vehicleEnvironmentAtlasUv(vec3 eyeDirection) {
    vec3 d = uEnvironmentViewRight * eyeDirection.x +
        uEnvironmentViewUp * eyeDirection.y + uEnvironmentViewBack * eyeDirection.z;
    vec3 a = abs(d);
    vec2 faceUv;
    float face;
    if (a.x >= a.y && a.x >= a.z) {
        face = d.x >= 0.0 ? 0.0 : 1.0;
        faceUv = vec2(d.x >= 0.0 ? -d.z : d.z, -d.y) / max(a.x, 0.00001);
    } else if (a.y >= a.z) {
        face = d.y >= 0.0 ? 2.0 : 3.0;
        faceUv = vec2(d.x, d.y >= 0.0 ? d.z : -d.z) / max(a.y, 0.00001);
    } else {
        face = d.z >= 0.0 ? 4.0 : 5.0;
        faceUv = vec2(d.z >= 0.0 ? d.x : -d.x, -d.y) / max(a.z, 0.00001);
    }
    float inset = 0.5 / max(uEnvironmentFaceSize, 1.0);
    faceUv = clamp(faceUv * 0.5 + vec2(0.5), vec2(inset), vec2(1.0 - inset));
    return (vec2(mod(face, 3.0), floor(face / 3.0)) + faceUv) / vec2(3.0, 2.0);
}

uniform float uNativeMaterialEffect;
varying vec3 vMaterialReflection;
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform int uDualTextureUvSet;
uniform int uDualTextureMode;
uniform float uDualTextureStrength;
uniform vec3 uDualTextureScale;
uniform int uUseTexture;
uniform int uRenderMode;
uniform vec3 uFogColor;
uniform int uPrelightMode;

varying vec4 vColor;
varying vec2 vTex0;
varying vec2 vTex1;
varying vec3 vLighting;
varying vec3 vSpecular;
varying float vFog;
void main(){
  vec4 texel = (uUseTexture != 0) ? texture2D(tex0, vTex0) : vec4(1.0);
  vec3 environmentReflection = vec3(0.0);
  if (uDualTextureMode != 0) {
    vec4 second = texture2D(tex1, (uDualTextureMode == 4 ? vehicleEnvironmentAtlasUv(vNativeEnvironmentDirection) :
      (uDualTextureMode == 2 ? vNativeEnvironmentUV : (uDualTextureUvSet != 0 ? vTex1 : vTex0)) * uDualTextureScale.xy));
    float amount = clamp(uDualTextureStrength, 0.0, 1.0);
    if (uDualTextureMode == 1) texel.rgb *= mix(vec3(1.0), second.rgb, amount);
    else if (uDualTextureMode == 2 || uDualTextureMode == 4) environmentReflection = second.rgb * second.a * amount;
    else if (uDualTextureMode == 3) texel.rgb = mix(texel.rgb, second.rgb, second.a * amount);
  }
  vec3 runtime = clamp(vLighting, 0.0, 1.18);
  vec3 vertexColor = clamp(vColor.rgb, 0.0, 1.0);
  vec3 lit = texel.rgb;
  float outAlpha = texel.a * clamp(vColor.a, 0.0, 1.0);

  if(uPrelightMode == 1){
    lit = texel.rgb * vertexColor;
  } else if(uPrelightMode == 2){
    lit = texel.rgb;
  } else if(uRenderMode == 0 || uRenderMode == 2){
    vec3 authored = texel.rgb * vertexColor;
    lit = authored * mix(vec3(1.0), runtime, 0.140) + vSpecular * mix(vec3(1.0), texel.rgb, 0.20) * texel.a;
  } else {
    lit = texel.rgb * vertexColor;
  }

  if (uPrelightMode == 0 && (uRenderMode == 0 || uRenderMode == 2))
    lit += environmentReflection;

  if (uPrelightMode == 0 && (uRenderMode == 0 || uRenderMode == 2))
    lit += vMaterialReflection * ((uDualTextureMode == 2 || uDualTextureMode == 4) ? 0.0 : clamp(uNativeMaterialEffect, 0.0, 1.0)) * texel.a;

  if (uPrelightMode == 0 && (uRenderMode == 0 || uRenderMode == 2)) {
    float luminance = dot(lit, vec3(0.2126, 0.7152, 0.0722));
    lit = mix(vec3(luminance), lit, 1.010);
    vec3 excess = max(lit - vec3(1.0), vec3(0.0));
    lit = lit / (vec3(1.0) + excess * 0.55);
  }
  if(uPrelightMode != 1){
    lit = mix(uFogColor, clamp(lit, 0.0, 1.12), vFog);
  }
  gl_FragColor = vec4(clamp(lit, 0.0, 1.0), outAlpha);
}
