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

uniform sampler2D tex0,tex1;
uniform int uUseTexture,uDualTextureMode,uDualTextureUvSet,uPrelightMode;
uniform vec3 uDualTextureScale,uAmbientColor,uDirectionalColor,uFogColor,uLightEye;
uniform vec3 uAeroSkyTop,uAeroSkyHorizon;
uniform float uFogStart,uFarClip,uDualTextureStrength,uAeroWet,uNativeMaterialEffect;
varying vec4 vColor;
varying vec2 vTex0,vTex1;
varying vec3 vEye,vNormal;
void main() {
 vec4 base=uUseTexture!=0?texture2D(tex0,vTex0):vec4(1.0);
 vec3 environmentReflection=vec3(0.0);
 if(uDualTextureMode!=0){
  vec4 detail=texture2D(tex1,(uDualTextureMode==4?vehicleEnvironmentAtlasUv(vNativeEnvironmentDirection):
      (uDualTextureMode==2?vNativeEnvironmentUV:(uDualTextureUvSet!=0?vTex1:vTex0))*uDualTextureScale.xy));
  float blend=clamp(uDualTextureStrength,0.0,1.0);
  if(uDualTextureMode==1)base.rgb*=mix(vec3(1.0),detail.rgb,blend);
  if(uDualTextureMode==2||uDualTextureMode==4)environmentReflection=detail.rgb*detail.a*blend;
  if(uDualTextureMode==3)base.rgb=mix(base.rgb,detail.rgb,detail.a*blend);
 }
 vec3 n=normalize(vNormal);
 vec3 light=normalize(uLightEye),view=normalize(-vEye);
 float diffuse=max(dot(n,light),0.0);
 // SA baked vertex colours and timecycle object ambience remain separate.
 vec3 vertexLighting=clamp(vColor.rgb,vec3(0.0),vec3(1.0));
 vec3 lit=base.rgb*vertexLighting*(max(uAmbientColor,vec3(.075))+uDirectionalColor*diffuse)+environmentReflection;
 if(uPrelightMode==1)lit=base.rgb*vertexLighting;
 float spec=pow(max(dot(n,normalize(light+view)),0.0),42.0);
 if(uNativeMaterialEffect>0.0)lit+=uDirectionalColor*spec*uNativeMaterialEffect*.38;
 float fog=clamp((length(vEye)-uFogStart)/max(uFarClip-uFogStart,.001),0.0,1.0);
 gl_FragColor=vec4(clamp(mix(lit,uFogColor,fog*fog*(3.0-2.0*fog)),0.0,1.0),base.a*vColor.a);
}
