#version 120
varying vec2 vNativeEnvironmentUV;
varying vec3 vNativeEnvironmentDirection;
varying vec4 vColor;
varying vec2 vTex0, vTex1;
varying vec3 vEye,vNormal;
varying vec4 vShadow,vTangent;
void main(){
    vec3 nativeNormal=normalize(gl_NormalMatrix*gl_Normal+vec3(0.000001));
    vec3 nativeEye=(gl_ModelViewMatrix*gl_Vertex).xyz;
    vec3 nativeIncident=normalize(nativeEye+vec3(0.000001));
    vec3 nativeReflection=reflect(nativeIncident,nativeNormal);
    vNativeEnvironmentDirection=nativeReflection;
    float nativeDenominator=2.0*sqrt(dot(nativeReflection.xy,nativeReflection.xy)+
        (nativeReflection.z+1.0)*(nativeReflection.z+1.0));
    vNativeEnvironmentUV=clamp(nativeReflection.xy/max(nativeDenominator,0.00001)+vec2(0.5),0.0,1.0);

 vec4 eye=gl_ModelViewMatrix*gl_Vertex;
 gl_Position=gl_ProjectionMatrix*eye;
 // UV4 carries the tangent and bitangent sign from MikkTSpace.
 vTangent=vec4(mat3(gl_ModelViewMatrix)*gl_MultiTexCoord4.xyz,gl_MultiTexCoord4.w); vNormal=gl_NormalMatrix*gl_Normal; vEye=eye.xyz; vShadow=gl_TextureMatrix[2]*eye;
 vColor=gl_Color; vTex0=gl_MultiTexCoord0.xy; vTex1=gl_MultiTexCoord1.xy;
}
