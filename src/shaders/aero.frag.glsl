#version 120
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
// Aero: OpenGL port of reLCS v6.9.45 world AO and sun-shadow receiving.
// Native depth textures replace reLCS's D3D9 RGB24 packing/half-texel offset.
uniform sampler2D tex0,tex1,uShadowMap;
uniform vec3 uDitherTexel;
uniform int uUseTexture,uPrelightMode,uDualTextureMode,uDualTextureUvSet,uShadowReady;
uniform vec3 uDualTextureScale,uAmbientColor,uDirectionalColor,uFogColor,uLightEye;
uniform vec3 uAeroSkyTop,uAeroSkyHorizon,uAeroSkyGround,uAeroSunColor;
uniform float uShadowTexel;
uniform float uDualTextureStrength,uFogStart,uFarClip,uAoStrength,uGloss;
uniform int uDeferred;
uniform float uAeroSurface,uNativeMaterialEffect,uSunVisibility;
uniform float uHeightScale,uEmissiveStrength,uMoonVisibility,uMafia2Tone;
uniform int uAeroWorld,uShadowWide;
// @aero_light@
varying vec4 vColor,vShadow,vTangent;
uniform int uMikkTangent;
varying vec2 vTex0,vTex1;
varying vec3 vEye,vNormal;
uniform sampler2D uNormalMap;
uniform int uHasNormalMap,uVertexNormal;
float shadow(float ndl){
 if(uShadowReady==0||vShadow.w<=0.0||ndl<=0.0)return 1.0;
 vec3 p=vShadow.xyz/vShadow.w*0.5+0.5;
 if(any(lessThan(p,vec3(0.0)))||any(greaterThan(p,vec3(1.0))))return 1.0;

 // Work out the receiver slope in shadow UV. Each tap checks its own depth.
 vec3 dx=dFdx(p),dy=dFdy(p);
 float det=dx.x*dy.y-dx.y*dy.x;
 vec2 plane=vec2(0.0);
 if(abs(det)>1e-12)plane=vec2(dx.z*dy.y-dy.z*dx.y,dy.z*dx.x-dx.z*dy.x)/det;
 plane=clamp(plane,vec2(-.65),vec2(.65));
 float slope=sqrt(max(1.0-ndl*ndl,0.0))/max(ndl,.16);
 float bias=uShadowTexel*(.16+.16*min(slope,3.0));
 vec2 pixel=p.xy/uShadowTexel-.5;
 vec2 center=(floor(pixel)+.5)*uShadowTexel;
 vec2 fraction=fract(pixel);
 float lit=0.0;
 float totalWeight=0.0;
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  if(uShadowWide==0&&(x<0||y<0))continue;
  vec2 tap=vec2(float(x),float(y));
  vec2 offset=center+tap*uShadowTexel-p.xy;
  vec2 weight=uShadowWide==0?1.0-abs(tap-fraction):
      max(vec2(0.0),vec2(1.5)-abs(tap-fraction));
  float w=weight.x*weight.y;
  float depth=texture2D(uShadowMap,p.xy+offset).r;
  lit+=w*step(p.z+dot(plane,offset)-bias,depth);
  totalWeight+=w;
 }
 float edge=max(abs(p.x-.5),abs(p.y-.5))*2.0;
 float coverage=lit/max(totalWeight,.001);
 return 1.0-(1.0-coverage)*.84*(1.0-smoothstep(.80,.99,edge))*max(uSunVisibility,uMoonVisibility*.8);
}

vec3 aeroEnvironment(vec3 reflection,float gloss,float visibility){
 float z=dot(reflection,safeNormal(uUpEye));
 vec3 sky=mix(uAeroSkyHorizon,uAeroSkyTop,smoothstep(0.0,.95,z));
 vec3 ground=mix(uAeroSkyHorizon,uAeroSkyGround,smoothstep(0.0,.7,-z));
 float sun=pow(max(dot(reflection,safeNormal(uLightEye)),0.0),mix(6.0,220.0,gloss));
 return mix(ground,sky,smoothstep(-.14,.14,z))+uAeroSunColor*sun*visibility*max(uSunVisibility,uMoonVisibility*.15)*.22;
}
void main(){
 vec3 view=safeNormal(-vEye);
 vec3 n=safeNormal(cross(dFdx(vEye),dFdy(vEye)));
 if(dot(n,view)<0.0)n=-n;
 vec3 geometric=n;
 if(uVertexNormal!=0&&dot(vNormal,vNormal)>1e-8){n=safeNormal(vNormal);if(dot(n,geometric)<0.0)n=-n;}
 vec3 tangent=safeNormal(vTangent.xyz-n*dot(n,vTangent.xyz));
 vec3 bitangent=vTangent.w*cross(n,tangent);
 vec2 sampleUv=vTex0;
 if(uHasNormalMap!=0 && uHeightScale>0.0 && uUseTexture!=0){
  // The generated RGBA normal texture stores actual source-derived height in A.
  vec3 viewTangent=vec3(dot(view,tangent),dot(view,bitangent),dot(view,n));
  if(uMikkTangent==0){
   vec3 dpx=dFdx(vEye),dpy=dFdy(vEye);
   vec2 dux=dFdx(vTex0),duy=dFdy(vTex0);
   float uvDet=dux.x*duy.y-duy.x*dux.y;
   if(abs(uvDet)>1e-8){
    tangent=safeNormal((dpx*duy.y-dpy*dux.y)/uvDet);
    tangent=safeNormal(tangent-n*dot(tangent,n));
    bitangent=safeNormal(cross(n,tangent))*sign(uvDet);
    viewTangent=vec3(dot(view,tangent),dot(view,bitangent),dot(view,n));
   }
  }
  float h=texture2D(uNormalMap,sampleUv).a;
  vec2 parallax=(viewTangent.xy/max(abs(viewTangent.z),0.3))*uHeightScale;
  sampleUv+=parallax*(h-.5);
  if(uLightingModel==2){
   float h2=texture2D(uNormalMap,sampleUv).a;
   sampleUv+=parallax*(h2-h)*.5;
  }
 }
 vec4 base=uUseTexture!=0?texture2D(tex0,sampleUv):vec4(1.0);
 // Only reconstruct magnified, opaque palette checkerboards. Quantized
 // sample coordinates have discontinuous derivatives; using them while
 // minifying selected unstable mip levels and made surfaces shimmer.
 vec2 texel=max(uDitherTexel.xy,vec2(1e-8));
 float footprint=max(length(dFdx(sampleUv)/texel),length(dFdy(sampleUv)/texel));
 if(uUseTexture!=0&&uDitherTexel.z>0.0&&footprint<1.0){
  vec2 uv=(floor(sampleUv/texel)+.5)*texel;
  vec4 a=texture2D(tex0,uv),b=texture2D(tex0,uv+vec2(texel.x,0.0));
  vec4 c=texture2D(tex0,uv+vec2(0.0,texel.y)),d=texture2D(tex0,uv+texel);
  float diagonal=max(length(a.rgb-d.rgb),length(b.rgb-c.rgb));
  float contrast=length(a.rgb-b.rgb);
  float opaque=min(min(a.a,b.a),min(c.a,d.a));
  if(diagonal<.012&&contrast>.004&&contrast<.065&&opaque>.98){
   float magnified=1.0-smoothstep(.5,1.0,footprint);
   base.rgb=mix(base.rgb,(a.rgb+b.rgb+c.rgb+d.rgb)*.25,magnified);
  }
 }

 vec3 environmentReflection=vec3(0.0);
 if(uDualTextureMode!=0){
  vec3 nativeIncident=normalize(vEye+vec3(0.000001));
  vec3 nativeReflection=reflect(nativeIncident,normalize(vNormal+vec3(0.000001)));
  float nativeDenominator=2.0*sqrt(dot(nativeReflection.xy,nativeReflection.xy)+
      (nativeReflection.z+1.0)*(nativeReflection.z+1.0));
  vec2 nativeEnvironmentUV=clamp(nativeReflection.xy/max(nativeDenominator,0.00001)+vec2(0.5),0.0,1.0);
  vec4 t=texture2D(tex1,(uDualTextureMode==4?vehicleEnvironmentAtlasUv(nativeReflection):
      (uDualTextureMode==2?nativeEnvironmentUV:(uDualTextureUvSet!=0?vTex1:sampleUv))*uDualTextureScale.xy));
  float a=clamp(uDualTextureStrength,0.0,1.0);
  if(uDualTextureMode==1)base.rgb*=mix(vec3(1),t.rgb,a);
  if(uDualTextureMode==2||uDualTextureMode==4)environmentReflection=t.rgb*t.a*a;
  if(uDualTextureMode==3)base.rgb=mix(base.rgb,t.rgb,t.a*a);
 }
 if(uHasNormalMap!=0){
  vec3 dx=dFdx(vEye),dy=dFdy(vEye);vec2 tx=dFdx(sampleUv),ty=dFdy(sampleUv);
  float det=tx.x*ty.y-tx.y*ty.x;
  if(uMikkTangent!=0){
   vec3 t=safeNormal(vTangent.xyz-n*dot(n,vTangent.xyz)),b=vTangent.w*cross(n,t);
   vec3 ts=texture2D(uNormalMap,sampleUv).xyz*2.0-1.0;n=safeNormal(t*ts.x+b*ts.y+n*ts.z);
  }else if(abs(det)>1e-8){
   vec3 t=(dx*ty.y-dy*tx.y)/det,b=(dy*tx.x-dx*ty.x)/det;
   t=safeNormal(t-n*dot(n,t));b=safeNormal(b-n*dot(n,b)-t*dot(t,b));
   vec3 ts=texture2D(uNormalMap,sampleUv).xyz*2.0-1.0;n=safeNormal(t*ts.x+b*ts.y+n*ts.z);
  }
 }
 vec3 light=safeNormal(uLightEye),up=safeNormal(uUpEye);
 float ndl=max(dot(n,light),0.0);
 float wet=uAeroWet*float(uAeroWorld)*smoothstep(.48,.93,dot(geometric,up));
 float reflection=clamp(max(uAeroSurface,uNativeMaterialEffect),0.0,1.0);
 float gloss=clamp(uGloss+reflection*.57+wet*.59+uAeroWet*uAeroSurface*.16,0.0,1.0);

 // Keep baked prelight in the ambient term so the sun can still hit dark paint.
 float ao=mix(0.72,1.0,clamp(dot(clamp(vColor.rgb,0.0,1.0),vec3(.299,.587,.114))*1.30,0.0,1.0));
 ao*=mix(0.78,1.0,clamp(dot(n,up)*.5+.5,0.0,1.0));
 ao=mix(1.0,ao,uAoStrength);
 float visibility=shadow(max(dot(geometric,light),0.0));
 float specAngle=uLightingModel==1?max(dot(reflect(-light,n),view),0.0)
                              :max(dot(n,safeNormal(light+view)),0.0);
 float power=uLightingModel==1?mix(8.0,50.0,gloss):mix(12.0,110.0,gloss);
 float spec=uLightingModel==0?0.0:pow(specAngle,power)*gloss*ndl*visibility;
 vec3 surface=base.rgb*(1.0-wet*.24);
 vec3 sunlight=uDirectionalColor*max(uSunVisibility,.38*uMoonVisibility);
 vec3 lit=surface*(vColor.rgb*(uAmbientColor+vec3(.045))*ao+sunlight*ndl*visibility);
 lit+=sunlight*spec;
 float grazing=pow(1.0-clamp(dot(n,view),0.0,1.0),5.0);
 float fresnel=.04+grazing*.96;
 float reflectivity=(.026+reflection*.52+wet*.31)*(.18+fresnel*.82);
 if(uDualTextureMode==2||uDualTextureMode==4)lit+=environmentReflection;
 else lit=mix(lit,aeroEnvironment(reflect(-view,n),gloss,visibility),reflectivity);
 if(uDeferred==0)lit+=aeroPointLight(vEye,n,view,surface,gloss,wet);
 // Emissive textures still emit after sunset; their colour is never taken
 // from an unlit white proxy, and alpha remains the original texture alpha.
 float emission=uEmissiveStrength*(.35+.65*(1.0-uSunVisibility));
 lit+=base.rgb*emission;
 if(uMafia2Tone>0.5){
  vec3 film=lit/(lit+vec3(.34));
  lit=mix(lit,film*.90,0.55);
  lit=mix(vec3(dot(lit,vec3(.2126,.7152,.0722))),lit,0.87);
 }
 if(uPrelightMode==1)lit=base.rgb*vColor.rgb;
 float fog=clamp((uFarClip-length(vEye))/max(uFarClip-uFogStart,.001),0.0,1.0);
 if(uPrelightMode!=1)lit=mix(uFogColor,lit,fog*fog*(3.0-2.0*fog));
 gl_FragData[0]=vec4(clamp(lit,0.0,1.0),base.a*vColor.a);
 if(uDeferred!=0){
  gl_FragData[1]=vec4(n,gloss);
  gl_FragData[2]=vec4(surface,fog*fog*(3.0-2.0*fog));
  gl_FragData[3]=vec4(vEye,base.a*vColor.a>.98&&uPrelightMode!=1?
      1.0+float(uAeroWorld)+(emission>.02?.25:0.0):0.0);
 }

}
