uniform vec3 uConsoleAmbient;
uniform vec3 uConsoleEmissive;
uniform vec3 uAmbientColor;
uniform vec3 uDirectionalColor;
uniform vec3 uLightEye;
uniform vec3 uPspExtraLightEye0;
uniform vec3 uPspExtraLightEye1;
uniform vec3 uPspExtraLightColor0;
uniform vec3 uPspExtraLightColor1;
uniform float uFogStart;
uniform float uFarClip;
uniform float uPspFogSlope;
uniform int uRenderMode;
uniform int uPrelightMode;
uniform int uPspPrelitModel;
uniform float uPspDepthOffset;
uniform vec3 uPspEnvironmentS;
uniform vec3 uPspEnvironmentT;

varying vec4 vPspColor;
varying float vPspFog;
varying float vPspClipW;
varying vec2 vTex0;
varying vec2 vTex1;
varying vec2 vNativeEnvironmentUV;
varying vec3 vPspEnvironmentColor;

vec3 pspLightProduct(vec3 light, vec3 material) {
    return floor((2.0 * light + 1.0) * (2.0 * material + 1.0) / 1024.0);
}

vec3 pspLightScale(vec3 color, float factor) {
    float amount = floor(clamp(factor, 0.0, 1.0) * 256.0);
    return floor((2.0 * color + 1.0) * (2.0 * amount + 1.0) / 1024.0);
}

vec3 pspLightBytes(vec3 color) {
    return floor(clamp(color, 0.0, 1.0) * 255.0 + 0.0001);
}

vec3 pspDirectionalLight(vec3 normal, vec3 direction, vec3 color, vec3 material) {
    float normalLengthSquared = dot(normal, normal);
    float lightLengthSquared = dot(direction, direction);
    float diffuse = 0.0;
    if (normalLengthSquared > 0.0 && lightLengthSquared > 0.0) {
        diffuse = max(dot(normal, direction)
            * inversesqrt(normalLengthSquared * lightLengthSquared), 0.0);
    }
    return pspLightScale(pspLightProduct(pspLightBytes(color), material), diffuse);
}

void main() {
    vec4 eyePosition = gl_ModelViewMatrix * gl_Vertex;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    gl_Position.z += uPspDepthOffset * gl_Position.w;
    vTex0 = gl_MultiTexCoord0.xy;
    vTex1 = gl_MultiTexCoord1.xy;

    vec4 material = floor(clamp(gl_Color, 0.0, 1.0) * 255.0 + 0.5);
    vec3 primary = material.rgb;
    if (uPrelightMode == 2) {
        primary = vec3(255.0);
    } else if (uPrelightMode == 0 && (uRenderMode == 0 || uRenderMode == 2)) {
        if (uPspPrelitModel != 0) {
            primary = material.rgb + pspLightBytes(uConsoleEmissive);
        } else {
#if STORYLAND_PSP_WORLD
        primary = pspLightProduct(pspLightBytes(uConsoleAmbient.bgr), material.rgb)
            + pspLightBytes(uConsoleEmissive);
#else
        // The GE uses the world matrix, not the inverse transpose. A rigid
        // camera rotation preserves this dot product in eye space.
        vec3 normal = mat3(gl_ModelViewMatrix) * gl_Normal;
        primary = pspLightProduct(pspLightBytes(uAmbientColor), material.rgb)
            + pspDirectionalLight(normal, uLightEye, uDirectionalColor, material.rgb)
            + pspDirectionalLight(normal, uPspExtraLightEye0, uPspExtraLightColor0, material.rgb)
            + pspDirectionalLight(normal, uPspExtraLightEye1, uPspExtraLightColor1, material.rgb);
#endif
        }
    }

    vec3 environmentNormal = mat3(gl_ModelViewMatrix) * gl_Normal;
    float environmentLengthSquared = dot(environmentNormal, environmentNormal);
    if (environmentLengthSquared > 0.0)
        environmentNormal *= inversesqrt(environmentLengthSquared);
    vec2 environmentDots = vec2(dot(environmentNormal,uPspEnvironmentS),
        dot(environmentNormal,uPspEnvironmentT));
    vNativeEnvironmentUV = environmentDots * 0.5 + 0.5;
    vec3 environmentColor = pspLightBytes(uAmbientColor)
        + pspLightScale(vec3(255.0),environmentDots.x)
        + pspLightScale(vec3(255.0),environmentDots.y);
    vPspEnvironmentColor = clamp(environmentColor,0.0,255.0) * gl_Position.w;

    float fogRange = max(uFarClip - uFogStart, 0.000001);
    float fogSlope = uPspFogSlope > 0.0 ? uPspFogSlope : 1.0 / fogRange;
    float fog = clamp((uFarClip + eyePosition.z) * fogSlope, 0.0, 1.0);
    float fogByte = min(floor(fog * 256.0), 255.0);

    // GLSL 1.20 has no noperspective qualifier. The quotient in the fragment
    // stage restores the GE's screen-linear colors and fog; UVs stay projected.
    vPspClipW = gl_Position.w;
    vPspColor = vec4(clamp(primary, 0.0, 255.0), material.a) * gl_Position.w;
    vPspFog = fogByte * gl_Position.w;
}
