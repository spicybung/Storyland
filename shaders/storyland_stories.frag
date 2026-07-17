#version 120
uniform sampler2D tex0;
uniform int uUseTexture;
uniform int uRenderMode;
uniform vec3 uFogColor;
uniform vec3 uDirectionalColor;
varying vec4 vColor;
varying vec2 vTex0;
varying vec3 vLighting;
varying float vSpecular;
varying float vFog;
void main(){
  vec4 base = vec4(vColor.rgb, 1.0);
  if(uUseTexture != 0){ vec4 tex = texture2D(tex0, vTex0); base.rgb *= tex.rgb; }
  vec3 lit = base.rgb;
  if(uRenderMode == 0 || uRenderMode == 2){
    lit = base.rgb * vLighting + vSpecular * uDirectionalColor;
  }
  lit = mix(uFogColor, clamp(lit, 0.0, 1.0), vFog);
  gl_FragColor = vec4(lit, 1.0);
}
