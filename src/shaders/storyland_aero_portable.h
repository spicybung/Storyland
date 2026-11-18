#pragma once

inline constexpr const char* StorylandPortableAeroVertex=R"GLSL(#version 120
varying vec4 vColor;
varying vec2 vTex0;
uniform mat4 uEyeToShadow;
varying vec4 vShadow;
varying vec3 vEye,vNormal;
void main() {
 vec4 eye=gl_ModelViewMatrix*gl_Vertex;
 vShadow=uEyeToShadow*eye;
 gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;
 vEye=eye.xyz;
 vNormal=normalize(gl_NormalMatrix*gl_Normal);
 vColor=gl_Color;
 vTex0=gl_MultiTexCoord0.xy;
})GLSL";

inline constexpr const char* StorylandPortableAeroFragment=R"GLSL(#version 120
uniform sampler2D tex0,tex1;
uniform int uUseTexture,uLightingModel,uShadowEnabled,uDualTextureMode;
uniform float uDualTextureStrength;
uniform sampler2D uShadowMap;
uniform float uShadowTexel;
varying vec4 vShadow;
uniform float uHeightScale,uEmissiveStrength,uMafia2Tone,uNightStrength;
uniform vec3 uAmbientColor,uDirectionalColor,uFogColor,uLightEye;
uniform float uFogStart,uFarClip;
uniform int uEmissionPointCount;
uniform vec3 uEmissionPointPosition[8],uEmissionPointColor[8];
uniform float uEmissionPointRange[8];
varying vec4 vColor;
varying vec2 vTex0;
varying vec3 vEye,vNormal;
void main() {
 vec3 view=normalize(-vEye);
 vec3 n=normalize(vNormal);
 vec2 uv=vTex0;
 if(uUseTexture!=0&&uHeightScale>.001) {
  // Source-dependent relief: derive height from texture luminance.
  // Never change transparent edges, as that would pull alpha-tested cut-outs apart.
  vec4 sampleHeight=texture2D(tex0,uv);
  if(sampleHeight.a>.985) {
   float height=dot(sampleHeight.rgb,vec3(.2126,.7152,.0722));
   float oblique=(1.0-max(dot(n,view),0.0));
   uv+=normalize(vEye.xy+vec2(.001))*(.5-height)*uHeightScale*oblique;
  }
 }
 vec4 sampled=uUseTexture!=0?texture2D(tex0,uv):vec4(1.0);
 if(uUseTexture!=0&&uDualTextureMode==2){
  vec3 reflection=reflect(normalize(vEye+vec3(0.000001)),n);
  float denominator=2.0*sqrt(dot(reflection.xy,reflection.xy)+
      (reflection.z+1.0)*(reflection.z+1.0));
  vec2 uvEnvironment=clamp(reflection.xy/max(denominator,0.00001)+vec2(.5),0.0,1.0);
  vec4 environment=texture2D(tex1,uvEnvironment);
  sampled.rgb+=environment.rgb*environment.a*clamp(uDualTextureStrength,0.0,1.0);
 }
 vec3 light=normalize(uLightEye);
 float ndl=max(dot(n,light),0.0);
 float shadow=1.0;
 if(uShadowEnabled!=0&&ndl>.001&&vShadow.w>.0){
  vec3 coord=vShadow.xyz/vShadow.w*.5+.5;
  if(coord.x>.001&&coord.y>.001&&coord.x<.999&&coord.y<.999&&coord.z<.999){
   float visibility=0.0;
   float bias=.0015+uShadowTexel*(1.0-ndl)*.85;
   for(int y=-1;y<=1;++y){
    for(int x=-1;x<=1;++x){
     if(uLightingModel<2&&(x>0||y>0))continue;
     vec3 encoded=texture2D(uShadowMap,coord.xy+vec2(float(x),float(y))*uShadowTexel).rgb;
     float depth=dot(encoded,vec3(1.0,1.0/255.0,1.0/65025.0));
     visibility+=step(coord.z-bias,depth);
    }
   }
   shadow=.38+.62*(visibility/(uLightingModel<2?4.0:9.0));
  }
 }
 vec3 ambient=max(uAmbientColor,vec3(.018));
 ambient=max(ambient,uNightStrength*vec3(.07,.085,.128));
 vec3 color=sampled.rgb*vColor.rgb*(ambient+uDirectionalColor*(ndl*shadow*.82+.10));
 for(int index=0;index<8;++index) {
  if(index>=uEmissionPointCount)break;
  vec3 delta=uEmissionPointPosition[index]-vEye;
  float distance=length(delta);
  float attenuation=max(1.0-distance/max(uEmissionPointRange[index],.001),0.0);
  vec3 direction=delta/max(distance,.001);
  float diffuse=max(dot(n,direction),0.0);
  color+=sampled.rgb*vColor.rgb*uEmissionPointColor[index]*(diffuse*attenuation*attenuation);
 }
 if(uLightingModel>0) {
  float phong=pow(max(dot(reflect(-light,n),view),0.0),36.0);
  float blinn=pow(max(dot(n,normalize(light+view)),0.0),56.0);
  float specular=uLightingModel==1?phong:blinn;
  float fresnel=pow(1.0-max(dot(n,view),0.0),5.0);
  color+=uDirectionalColor*(specular*.31*shadow+fresnel*.022)*step(.98,sampled.a);
 }
 float emissive=clamp(uEmissiveStrength,0.0,1.1);
 if(emissive>0.0) {
  float lamp=pow(max(max(sampled.r,sampled.g),sampled.b),1.0);
  color+=sampled.rgb*emissive*(.38+.62*lamp);
 }
 float fog=smoothstep(uFogStart,max(uFogStart+.001,uFarClip),length(vEye));
 color=mix(color,uFogColor,fog);
 if(uMafia2Tone>.5) {
  float luminance=dot(color,vec3(.2126,.7152,.0722));
  vec3 vintage=mix(vec3(luminance),color,.73)*vec3(1.06,1.01,.92);
  color=mix(color,vintage,.65);
  color=color/(color+vec3(.42));
  color=pow(clamp(color*1.36,0.0,1.0),vec3(.92));
 }
 gl_FragColor=vec4(clamp(color,0.0,1.0),sampled.a*vColor.a);
})GLSL";

inline constexpr const char* StorylandPortableSanAndreasFragment=R"GLSL(#version 120
uniform sampler2D tex0;
uniform int uUseTexture;
uniform vec3 uAmbientColor,uDirectionalColor,uFogColor,uLightEye;
uniform float uFogStart,uFarClip;
varying vec4 vColor;
varying vec2 vTex0;
varying vec3 vEye,vNormal;
void main() {
 vec4 textureColor=uUseTexture!=0?texture2D(tex0,vTex0):vec4(1.0);
 float sun=max(dot(normalize(vNormal),normalize(uLightEye)),0.0);
 vec3 light=max(uAmbientColor,vec3(.13,.115,.12))+uDirectionalColor*(.15+.75*sun);
 vec3 color=textureColor.rgb*vColor.rgb*light;
 color=mix(color,vec3(.92,.79,.63)*dot(color,vec3(.32,.58,.10)),.07);
 float fog=smoothstep(uFogStart,max(uFogStart+.001,uFarClip),length(vEye));
 gl_FragColor=vec4(clamp(mix(color,uFogColor,fog),0.0,1.0),textureColor.a*vColor.a);
})GLSL";

inline constexpr const char* StorylandPortableShadowVertex=R"GLSL(#version 120
varying vec2 vShadowUv;
varying float vShadowAlpha;
void main() {
 gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;
 vShadowUv=gl_MultiTexCoord0.xy;
 vShadowAlpha=gl_Color.a;
}
)GLSL";

inline constexpr const char* StorylandPortableShadowFragment=R"GLSL(#version 120
uniform sampler2D tex0;
uniform int uUseTexture;
varying vec2 vShadowUv;
varying float vShadowAlpha;
void main() {
 float alpha=vShadowAlpha*(uUseTexture!=0?texture2D(tex0,vShadowUv).a:1.0);
 if(alpha<=.5)discard;
 float depth=min(gl_FragCoord.z,.999999);
 vec3 encoded=fract(depth*vec3(1.0,255.0,65025.0));
 encoded.xy-=encoded.yz/255.0;
 gl_FragColor=vec4(encoded,1.0);
}
)GLSL";

inline constexpr const char* StorylandPortableWaterVertex=R"GLSL(#version 120
uniform float time,wind;
varying vec3 worldPosition,eyePosition;
float waveHeight(vec2 p) {
 float strength=clamp(wind,0.0,1.0);
 return (.035+.080*strength)*sin(dot(p,vec2(.095,.034))+time*.72)
      + (.026+.048*strength)*sin(dot(p,vec2(-.041,.108))+time*.91+1.7)
      + (.070+.070*strength)*sin(dot(p,vec2(.018,.011))+time*.31+.9)
      + (.055+.050*strength)*sin(dot(p,vec2(-.013,.021))+time*.24+2.6);
}
void main() {
 vec4 point=gl_Vertex;
 point.z+=waveHeight(point.xy);
 worldPosition=point.xyz;
 eyePosition=(gl_ModelViewMatrix*point).xyz;
 gl_Position=gl_ModelViewProjectionMatrix*point;
})GLSL";

inline constexpr const char* StorylandPortableWaterFragment=R"GLSL(#version 120
uniform sampler2D waterTexture,reactionTexture;
uniform int textured,reactionEnabled;
uniform float time,wind,fogStart,fogEnd,rain,sunVisibility;
uniform vec3 cameraPosition,skyHorizon,skyZenith,skyLower,sunDirection,sunColor,fogColor;
uniform vec3 reactionOrigin,reactionInvExtent,reactionTexel;
varying vec3 worldPosition,eyePosition;
vec2 slope(vec2 p) {
 float strength=clamp(wind,0.0,1.0);
 vec2 result=(.035+.080*strength)*vec2(.095,.034)*cos(dot(p,vec2(.095,.034))+time*.72);
 result+=(.026+.048*strength)*vec2(-.041,.108)*cos(dot(p,vec2(-.041,.108))+time*.91+1.7);
 result+=(.070+.070*strength)*vec2(.018,.011)*cos(dot(p,vec2(.018,.011))+time*.31+.9);
 result+=(.055+.050*strength)*vec2(-.013,.021)*cos(dot(p,vec2(-.013,.021))+time*.24+2.6);
 result+=vec2(.034*cos(dot(p,vec2(.37,.18))+time*1.7),
              .028*cos(dot(p,vec2(-.17,.44))-time*1.31))*(.28+.72*strength);
 result+=vec2(.015*cos(dot(p,vec2(.93,-.71))+time*2.2),
              .019*cos(dot(p,vec2(-.68,-.95))+time*1.8))*(.25+.75*strength);
 return result;
}
vec3 react(vec2 p) {
 if(reactionEnabled==0)return vec3(0.0);
 vec2 uv=(p-reactionOrigin.xy)*reactionInvExtent.xy;
 if(uv.x<=.015||uv.y<=.015||uv.x>=.985||uv.y>=.985)return vec3(0.0);
 float center=texture2D(reactionTexture,uv).r;
 float dx=texture2D(reactionTexture,uv+vec2(reactionTexel.x,0)).r
         -texture2D(reactionTexture,uv-vec2(reactionTexel.x,0)).r;
 float dy=texture2D(reactionTexture,uv+vec2(0,reactionTexel.y)).r
         -texture2D(reactionTexture,uv-vec2(0,reactionTexel.y)).r;
 return vec3(center,dx,dy);
}
void main() {
 vec2 p=worldPosition.xy;
 vec3 reaction=react(p);
 vec2 waves=slope(p)+reaction.yz*.65;
 waves+=rain*.012*vec2(cos(dot(p,vec2(1.73,-1.29))+time*5.2),
                        cos(dot(p,vec2(-2.01,-1.54))-time*4.6));
 vec3 n=normalize(vec3(-waves,1));
 vec3 v=normalize(cameraPosition-worldPosition);
 if(v.z<0.0)n=-n;
 float fresnel=.02+.98*pow(1.0-max(dot(n,v),0.0),5.0);
 vec3 reflected=reflect(-v,n);
 vec3 sky=mix(skyHorizon,skyZenith,smoothstep(0.0,1.0,reflected.z));
 vec3 lower=mix(skyHorizon,skyLower,smoothstep(0.0,.65,-reflected.z));
 vec3 environment=mix(lower,sky,smoothstep(-.12,.14,reflected.z));
 vec3 detail=vec3(.5);
 if(textured!=0) {
  detail=texture2D(waterTexture,p*.028+time*vec2(.0042,.0029)).rgb*.48;
  detail+=texture2D(waterTexture,vec2(dot(p,vec2(.0211,.0174)),dot(p,vec2(-.0174,.0211)))
                  +time*vec2(-.0025,.0036)).rgb*.32;
  detail+=texture2D(waterTexture,vec2(dot(p,vec2(.0487,-.0196)),dot(p,vec2(.0196,.0487)))
                  +time*vec2(.0017,-.0021)).rgb*.20;
 }
 vec3 ambient=max(.055*skyZenith+.085*skyHorizon,vec3(.01));
 vec3 body=ambient*vec3(.50,1.02,.88)*(.7+dot(detail,vec3(.16,.28,.10)));
 body=mix(body,skyLower*.23,clamp(rain*.36,0.0,.36));
 vec3 color=mix(body,environment,clamp(.12+fresnel*.85,0.0,1.0));
 vec3 light=normalize(sunDirection),h=normalize(light+v);
 float ndh=max(dot(n,h),0.0);
 color+=sunColor*(pow(ndh,mix(240.0,65.0,max(wind,rain)))*1.22+
                  pow(ndh,mix(46.0,18.0,max(wind,rain)))*.14)*max(light.z,0.0)*sunVisibility;
 color+=reaction.x*.075*mix(vec3(.22,.42,.45),skyHorizon,.45);
 float fog=smoothstep(fogStart,max(fogStart+.001,fogEnd),length(eyePosition));
 gl_FragColor=vec4(clamp(mix(color,fogColor,fog),0.0,1.0),clamp(.84+fresnel*.13,0.0,.98));
})GLSL";
