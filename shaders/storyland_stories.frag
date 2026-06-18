#version 120
uniform sampler2D tex0;
uniform int uUseTexture;
uniform int uRenderMode;
uniform vec3 uFogColor;
varying vec4 vColor;
varying vec2 vTex0;
varying vec3 vNormal;
varying float vFog;
void main(){
  vec4 base = vec4(vColor.rgb, 1.0);
  if(uUseTexture != 0){ vec4 tex = texture2D(tex0, vTex0); base.rgb *= tex.rgb; }
  vec3 n = normalize(vNormal);
  vec3 l0 = normalize(vec3(0.25, 0.55, 0.78));
  vec3 l1 = normalize(vec3(-0.60, -0.25, 0.50));
  float d0 = max(dot(n, l0), 0.0);
  float d1 = max(dot(n, l1), 0.0) * 0.32;
  vec3 v = vec3(0.0, 0.0, 1.0);
  float spec = pow(max(dot(reflect(-l0, n), v), 0.0), 18.0) * 0.14;
  vec3 lit = base.rgb;
  if(uRenderMode == 0 || uRenderMode == 2){
    lit = base.rgb * (0.62 + d0 * 0.55 + d1) + spec;
  }
  lit = mix(uFogColor, clamp(lit, 0.0, 1.0), vFog);
  gl_FragColor = vec4(lit, 1.0);
}
