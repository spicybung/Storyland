#version 120
varying vec2 vNativeEnvironmentUV;
// LCS PSP pedestrian program.
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
  if (uUseTexture != 0 && uDualTextureMode != 0) {
    vec4 second = texture2D(tex1, (uDualTextureMode == 2 ? vNativeEnvironmentUV : (uDualTextureUvSet != 0 ? vTex1 : vTex0)) * uDualTextureScale.xy);
    float amount = clamp(uDualTextureStrength, 0.0, 1.0);
    if (uDualTextureMode == 1) texel.rgb *= mix(vec3(1.0), second.rgb, amount);
    else if (uDualTextureMode == 2) texel.rgb += second.rgb * second.a * amount;
    else if (uDualTextureMode == 3) texel.rgb = mix(texel.rgb, second.rgb, second.a * amount);
  }
  vec3 runtime = clamp(vLighting, 0.0, 1.18);
  vec3 vertexColor = clamp(vColor.rgb, 0.0, 1.0);
  if (uPrelightMode == 0) vertexColor *= 1.0;
  vec3 lit = texel.rgb;
  float outAlpha = texel.a * clamp(vColor.a, 0.0, 1.0);

  if(uPrelightMode == 1){
    lit = texel.rgb * vertexColor;
  } else if(uPrelightMode == 2){
    lit = texel.rgb;
  } else if(uRenderMode == 0 || uRenderMode == 2){
    vec3 authored = texel.rgb * vertexColor;
    lit = authored * mix(vec3(1.0), runtime, 0.180) + vSpecular * mix(vec3(1.0), texel.rgb, 0.08) * texel.a;
  } else {
    lit = texel.rgb * vertexColor;
  }

  // Preserve native timecycle colours; console modulation saturates without a tone curve.
  if(uPrelightMode != 1){
    lit = mix(uFogColor, clamp(lit, 0.0, 1.0), vFog);
  }
  gl_FragColor = vec4(clamp(lit, 0.0, 1.0), outAlpha);
}
