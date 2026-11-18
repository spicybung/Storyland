#version 120
varying vec2 vNativeEnvironmentUV;
varying vec4 vColor;
varying vec2 vTex0;
varying vec2 vTex1;
varying vec3 vLighting;
varying vec3 vSpecular;
varying float vFog;
uniform vec3 uAmbientColor;
uniform vec3 uDirectionalColor;
uniform vec3 uSunDirection;
uniform float uFogStart;
uniform float uFarClip;
void main(){
    vec3 nativeNormal=normalize(gl_NormalMatrix*gl_Normal+vec3(0.000001));
    vec3 nativeEye=(gl_ModelViewMatrix*gl_Vertex).xyz;
    vec3 nativeIncident=normalize(nativeEye+vec3(0.000001));
    vec3 nativeReflection=reflect(nativeIncident,nativeNormal);
    float nativeDenominator=2.0*sqrt(dot(nativeReflection.xy,nativeReflection.xy)+
        (nativeReflection.z+1.0)*(nativeReflection.z+1.0));
    vNativeEnvironmentUV=clamp(nativeReflection.xy/max(nativeDenominator,0.00001)+vec2(0.5),0.0,1.0);

  vec4 eyePos = gl_ModelViewMatrix * gl_Vertex;
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  vColor = gl_Color;
  vTex0 = gl_MultiTexCoord0.xy;
  vTex1 = gl_MultiTexCoord1.xy;

  vec3 transformedNormal = gl_NormalMatrix * gl_Normal;
  vec3 normal = (dot(transformedNormal, transformedNormal) > 0.000001) ? normalize(transformedNormal) : vec3(0.0, 0.0, 1.0);
  vec3 worldLight = -uSunDirection;
  if(dot(worldLight, worldLight) <= 0.000001) worldLight = vec3(0.35, 0.25, 0.90);
  vec3 lightDir = normalize(gl_NormalMatrix * normalize(worldLight));
  float distanceFromCamera = length(eyePos.xyz);
  vec3 viewVector = -eyePos.xyz;
  vec3 viewDir = (dot(viewVector, viewVector) > 0.000001) ? normalize(viewVector) : vec3(0.0, 0.0, 1.0);
  float ndl = max(dot(normal, lightDir), 0.0);
  float wrapped = clamp(ndl * 0.72 + 0.28, 0.0, 1.0);
  float shadowFill = 1.0 - ndl;
  vec3 baseLight = uAmbientColor * 1.120
      + uDirectionalColor * (wrapped * 0.640 + shadowFill * 0.100)
      + vec3(0.055);
  vLighting = clamp(baseLight, vec3(0.240), vec3(1.18));

  vec3 halfVector = lightDir + viewDir;
  vec3 halfDir = (dot(halfVector, halfVector) > 0.000001) ? normalize(halfVector) : lightDir;
  float highlightFade = 1.0 / (1.0 + distanceFromCamera * 0.0015);
  float specular = pow(max(dot(normal, halfDir), 0.0), 18.0) * 0.025 * highlightFade;
  float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.5);
  float rim = fresnel * 0.024 * highlightFade;
  float lightFacing = clamp(ndl * 3.0, 0.0, 1.0);
  vSpecular = uDirectionalColor * (specular * lightFacing) + uAmbientColor * rim;

  float fogRange = max(uFarClip - uFogStart, 0.001);
  float fogLinear = clamp((uFarClip - distanceFromCamera) / fogRange, 0.0, 1.0);
  vFog = fogLinear * fogLinear * (3.0 - 2.0 * fogLinear);
}
