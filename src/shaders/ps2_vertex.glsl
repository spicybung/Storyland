uniform int uRenderMode;
uniform int uPrelightMode;
uniform int uPs2PrelitModel;
uniform vec4 uPs2WorldMultiplier;
uniform vec4 uPs2WorldAdditive;
uniform vec4 uPs2ColorCeiling;
uniform vec3 uPs2ActorAmbient;
uniform vec3 uPs2DirectionalColor;
uniform vec3 uLightEye;
uniform vec3 uPs2ExtraLightEye0;
uniform vec3 uPs2ExtraLightEye1;
uniform vec3 uPs2ExtraLightEye2;
uniform vec3 uPs2ExtraLightColor0;
uniform vec3 uPs2ExtraLightColor1;
uniform vec3 uPs2ExtraLightColor2;
uniform vec3 uPs2EnvironmentS;
uniform vec3 uPs2EnvironmentT;
uniform vec3 uPs2Fog;

#ifndef STORYLAND_PS2_BUDOKAI_REFLECTION
#define STORYLAND_PS2_BUDOKAI_REFLECTION 0
#endif

#if STORYLAND_PS2_BUDOKAI_REFLECTION
uniform int uPs2BudokaiReflection;
varying vec3 vPs2ReflectionNormal;
varying vec3 vPs2ReflectionView;
#endif

varying vec4 vPs2Color;
varying vec4 vPs2InspectionColor;
varying float vPs2Fog;
varying float vPs2ClipW;
varying vec2 vTex0;
varying vec2 vTex1;
varying vec2 vNativeEnvironmentUV;

void main() {
    vec4 eyePosition = gl_ModelViewMatrix * gl_Vertex;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    vec4 material = floor(clamp(gl_Color, 0.0, 1.0) * 255.0 + 0.5);
    vec4 primary = material;
    vec3 axisX = gl_ModelViewMatrix[0].xyz;
    vec3 axisY = gl_ModelViewMatrix[1].xyz;
    vec3 axisZ = gl_ModelViewMatrix[2].xyz;
    axisX /= max(length(axisX), 0.000001);
    axisY /= max(length(axisY), 0.000001);
    axisZ /= max(length(axisZ), 0.000001);
    vec3 normal = axisX * gl_Normal.x + axisY * gl_Normal.y + axisZ * gl_Normal.z;

#if STORYLAND_PS2_WORLD
    primary = floor(clamp(material * uPs2WorldMultiplier + uPs2WorldAdditive,
        vec4(0.0), uPs2ColorCeiling));
#else
    if (uPs2PrelitModel != 0) {
        primary = floor(clamp(material * uPs2WorldMultiplier + uPs2WorldAdditive,
            vec4(0.0), uPs2ColorCeiling));
    } else {
        vec3 materialScale = material.rgb * (1.0 / 510.0);
        vec3 color = (uPs2DirectionalColor * materialScale) * max(dot(normal, uLightEye), 0.0);
        color += (uPs2ExtraLightColor0 * materialScale) * max(dot(normal, uPs2ExtraLightEye0), 0.0);
        color += (uPs2ExtraLightColor1 * materialScale) * max(dot(normal, uPs2ExtraLightEye1), 0.0);
        color += (uPs2ExtraLightColor2 * materialScale) * max(dot(normal, uPs2ExtraLightEye2), 0.0);
        color += uPs2ActorAmbient * materialScale;
        primary.rgb = floor(clamp(color, vec3(0.0), uPs2ColorCeiling.rgb));
        primary.a = floor((material.a + 1.0) * 0.5);
    }
#endif

    if (uPrelightMode == 2 || uRenderMode == 1 || uRenderMode == 3)
        primary.rgb = vec3(128.0);

    vPs2ClipW = gl_Position.w;
    vPs2Color = primary * gl_Position.w;
    vPs2InspectionColor = gl_Color * gl_Position.w;
    vTex0 = gl_MultiTexCoord0.xy;
    vTex1 = gl_MultiTexCoord1.xy;
    vNativeEnvironmentUV = vec2(0.5) + vec2(dot(normal, uPs2EnvironmentS),
        dot(normal, uPs2EnvironmentT)) * 0.5;
#if STORYLAND_PS2_BUDOKAI_REFLECTION
    vec3 reflectionNormal = gl_NormalMatrix * gl_Normal;
    float reflectionNormalLength = length(reflectionNormal);
    vPs2ReflectionNormal = reflectionNormalLength > 0.00001
        ? reflectionNormal / reflectionNormalLength : vec3(0.0, 0.0, 1.0);
    vPs2ReflectionView = normalize(-eyePosition.xyz + vec3(0.00001));
    if (uPs2BudokaiReflection != 0) {
        vec3 reflected = reflect(-vPs2ReflectionView, vPs2ReflectionNormal);
        vNativeEnvironmentUV = reflected.xy * 0.5 + vec2(0.5);
    }
#endif
    float depth = -eyePosition.z;
    float fogFixed = floor(clamp(depth * uPs2Fog.x + uPs2Fog.y, 0.0, 255.0) * 16.0);
    vPs2Fog = floor(fogFixed / 16.0) * gl_Position.w;
}
