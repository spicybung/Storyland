#version 120
// Storyland vertex binding adapter for re3 Neo fragments.
// Original source: github.com/novawish/re3, 9a7fa478578beaba947ea867c15a25e411d641d8.
uniform vec3 uAmbientColor,uDirectionalColor,uSunDirection;
uniform float uFogStart,uFarClip,uReflection;
uniform int uNeoVehicle;
uniform vec3 uNeoLightDirection;
varying vec4 v_color,v_reflcolor;
varying vec2 v_tex0,v_tex1;
varying float v_fog;
// Neo reads the UVs and light inputs from the compatibility vertex state.
void main(){
 vec4 eye=gl_ModelViewMatrix*gl_Vertex;gl_Position=gl_ProjectionMatrix*eye;
 vec3 N=normalize(gl_NormalMatrix*gl_Normal),V=normalize(-eye.xyz);
 vec3 L=uNeoLightDirection/max(length(uNeoLightDirection),0.00001);
 v_color=gl_Color;v_color.rgb+=uAmbientColor+uDirectionalColor*max(dot(N,L),0.0);v_color=clamp(v_color,0.0,1.0);
 v_tex0=gl_MultiTexCoord0.xy;
 v_tex1=gl_MultiTexCoord1.xy;
 v_reflcolor=vec4(0.0);
 if(uNeoVehicle!=0){
  // Actor vertex colours carry material paint, rather than baked scene light.
  v_color.rgb=gl_Color.rgb*clamp(uAmbientColor+uDirectionalColor*max(dot(N,L),0.0),0.0,1.0);
  vec3 uv2=N*dot(V,N)*2.0-V;v_tex1=uv2.xy*.5+.5;
  float b=1.0-clamp(dot(V,N),0.0,1.0);
  v_reflcolor.a=mix(b*b*b*b*b,1.0,.5)*uReflection;
  v_reflcolor.rgb=pow(clamp(dot(N,normalize(V+L)),0.0,1.0),32.0)*uDirectionalColor*.2;
 }
 v_fog=clamp((uFarClip-length(eye.xyz))/max(uFarClip-uFogStart,.001),0.0,1.0);
}
