#version 120
uniform sampler2D sceneTex,normalTex,albedoTex,positionTex,sceneDepth,bloomTex;
uniform int uAoDirections,uSunRaySamples;
uniform int uBloomHighDefinition;
uniform vec3 uAoParameters,uSunScreen,uSunColor;
uniform float uAoStrength,uBloomStrength;
varying vec2 uv;
// @aero_light@
float sampleOcclusion(vec2 axis,float pixels,float radius,vec3 position,vec3 normal){
 float result=0.0;
 for(int ring=1;ring<=2;ring++){
  vec2 sampleUv=uv+axis*pixels*(float(ring)*0.5)/uAoParameters.xy;
  if(all(greaterThan(sampleUv,vec2(0)))&&all(lessThan(sampleUv,vec2(1)))){
   vec4 samplePosition=texture2D(positionTex,sampleUv);
   vec3 delta=samplePosition.xyz-position;
   float distanceSquared=dot(delta,delta);
   if(samplePosition.w>.5&&distanceSquared>radius*radius*.000225&&distanceSquared<radius*radius){
    float distance=sqrt(distanceSquared);
    float horizon=max(dot(normal,delta/distance)-.08,0.0);
    result+=horizon*(1.0-distance/radius);
   }
  }
 }
 return result;
}
void main(){
 vec4 scene=texture2D(sceneTex,uv),position=texture2D(positionTex,uv);
 if(position.w>.5){
  vec4 normal=texture2D(normalTex,uv),albedo=texture2D(albedoTex,uv);
  vec3 n=safeNormal(normal.xyz),view=safeNormal(-position.xyz);
  float wet=uAeroWet*step(1.5,position.w)*smoothstep(.48,.93,dot(n,safeNormal(uUpEye)));
  vec3 addition=aeroPointLight(position.xyz,n,view,albedo.rgb,normal.w,wet);
  float occlusion=0.0;
  float radius=max(uAoParameters.z,0.00001);
  float pixels=clamp(radius/max(abs(position.z),radius)*uAoParameters.y*0.5,2.0,48.0);
  if(uAoStrength>0.0&&uAoDirections>0){
   if(uAoDirections>=6){
    for(int direction=0;direction<8;direction++){
     if(direction>=uAoDirections)break;
     float angle=float(direction)*6.28318530718/float(uAoDirections);
     occlusion+=sampleOcclusion(vec2(cos(angle),sin(angle)),pixels,radius,position.xyz,n);
    }
   }else{
    for(int direction=0;direction<4;direction++){
     float angle=float(direction)*1.57079632679;
     occlusion+=sampleOcclusion(vec2(cos(angle),sin(angle)),pixels,radius,position.xyz,n);
    }
   }
  }
  float ao=clamp(1.0-uAoStrength*occlusion/max(float(uAoDirections)*2.0,1.0)*3.0,.25,1.0);
  scene.rgb*=mix(1.0,ao,albedo.a);
  scene.rgb+=addition*albedo.a;
 }
 if(uBloomStrength>.001){
  if(uBloomHighDefinition!=0){
   scene.rgb+=texture2D(bloomTex,uv).rgb*uBloomStrength;
  }else{
  vec2 pixel=1.0/max(uAoParameters.xy,vec2(1.0));
  vec3 glow=vec3(0.0);
  for(int sampleIndex=0;sampleIndex<4;sampleIndex++){
   vec2 direction=sampleIndex==0?vec2(1,0):sampleIndex==1?vec2(-1,0):
                  sampleIndex==2?vec2(0,1):vec2(0,-1);
   vec2 at=uv+direction*pixel*3.0;
   vec4 receiver=texture2D(positionTex,at);
   float emissive=step(.12,fract(receiver.w));
   glow+=texture2D(sceneTex,at).rgb*emissive;
  }
  scene.rgb+=glow*uBloomStrength*.25;
  }
 }
 // Trace back towards the sun through the opaque scene. Sky pixels let the light through.
 if(uSunRaySamples>0&&uSunScreen.z>0.0){
  vec2 toSun=uSunScreen.xy-uv;
  vec2 aspect=vec2(uAoParameters.x/uAoParameters.y,1.0);
  float distance=length(toSun*aspect);
  float spread=1.0-smoothstep(.07,.54,distance);
  if(spread>0.0){
   vec2 disc=vec2(.012/aspect.x,.012);
   float source=0.0;
   for(int axis=0;axis<4;axis++){
    vec2 offset=axis<2?vec2(disc.x,0.0):vec2(0.0,disc.y);
    vec2 at=uSunScreen.xy+offset*(mod(float(axis),2.0)*2.0-1.0);
    source+=step(.999995,texture2D(sceneDepth,at).r)*.25;
   }
   float openSky=0.0;
   float decay=1.0;
   for(int ray=0;ray<12;ray++){
    if(ray>=uSunRaySamples)break;
    float t=(float(ray)+.5)/float(uSunRaySamples);
    vec2 at=uv+toSun*t;
    if(all(greaterThan(at,vec2(0.0)))&&all(lessThan(at,vec2(1.0))))
     openSky+=step(.999995,texture2D(sceneDepth,at).r)*decay;
    decay*=.91;
   }
   float screenEdge=clamp((.56-max(abs(uSunScreen.x-.5),abs(uSunScreen.y-.5)))*20.0,0.0,1.0);
   float shafts=openSky/float(uSunRaySamples);
   scene.rgb+=uSunColor*shafts*source*spread*screenEdge*uSunScreen.z*.34;
  }
 }
 gl_FragColor=vec4(clamp(scene.rgb,0.0,1.0),scene.a);
}
