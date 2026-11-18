uniform sampler2D tex0;
uniform sampler2D tex1;
uniform int uUseTexture;
uniform int uRenderMode;
uniform int uPrelightMode;
uniform vec3 uFogColor;
uniform vec3 uPs2Fog;
uniform int uPs2TextureFunction;
uniform int uPs2TextureAlpha;
uniform int uPs2NativeTextureAlpha;
uniform int uDualTextureUvSet;
uniform int uDualTextureMode;
uniform float uDualTextureStrength;
uniform vec3 uDualTextureScale;
uniform float uPs2EnvironmentCoefficient;

varying vec4 vPs2Color;
varying vec4 vPs2InspectionColor;
varying float vPs2Fog;
varying float vPs2ClipW;
varying vec2 vTex0;
varying vec2 vTex1;
varying vec2 vNativeEnvironmentUV;

#ifndef STORYLAND_PS2_REFLECTION_ENHANCED
#define STORYLAND_PS2_REFLECTION_ENHANCED 0
#endif

#ifndef STORYLAND_PS2_BUDOKAI_REFLECTION
#define STORYLAND_PS2_BUDOKAI_REFLECTION 0
#endif

#if STORYLAND_PS2_BUDOKAI_REFLECTION
uniform int uPs2BudokaiReflection;
uniform vec3 uEnvironmentViewRight;
uniform vec3 uEnvironmentViewUp;
uniform vec3 uEnvironmentViewBack;
uniform float uEnvironmentFaceSize;
varying vec3 vPs2ReflectionNormal;
varying vec3 vPs2ReflectionView;

vec2 ps2BudokaiEnvironmentAtlasUv(vec3 eyeDirection) {
    vec3 direction = uEnvironmentViewRight * eyeDirection.x +
        uEnvironmentViewUp * eyeDirection.y + uEnvironmentViewBack * eyeDirection.z;
    vec3 magnitude = abs(direction);
    vec2 faceUv;
    float face;
    if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z) {
        face = direction.x >= 0.0 ? 0.0 : 1.0;
        faceUv = vec2(direction.x >= 0.0 ? -direction.z : direction.z, -direction.y)
            / max(magnitude.x, 0.00001);
    } else if (magnitude.y >= magnitude.z) {
        face = direction.y >= 0.0 ? 2.0 : 3.0;
        faceUv = vec2(direction.x, direction.y >= 0.0 ? direction.z : -direction.z)
            / max(magnitude.y, 0.00001);
    } else {
        face = direction.z >= 0.0 ? 4.0 : 5.0;
        faceUv = vec2(direction.z >= 0.0 ? direction.x : -direction.x, -direction.y)
            / max(magnitude.z, 0.00001);
    }
    float inset = 0.5 / max(uEnvironmentFaceSize, 1.0);
    faceUv = clamp(faceUv * 0.5 + vec2(0.5), vec2(inset), vec2(1.0 - inset));
    return (vec2(mod(face, 3.0), floor(face / 3.0)) + faceUv) / vec2(3.0, 2.0);
}

vec3 ps2BudokaiEnvironmentReflection(vec3 base, vec4 environment,
        float strength, float nativeAlphaScale) {
    float alpha = clamp(environment.a * nativeAlphaScale, 0.0, 1.0);
    vec3 reflected = environment.rgb * alpha * clamp(strength, 0.0, 1.0);
    return clamp(base + reflected * 255.0, 0.0, 255.0);
}
#endif

float ps2EnvironmentFix(float coefficient) {
    float fix = floor(clamp(coefficient, 0.0, 255.0) * 0.5);
#if STORYLAND_PS2_REFLECTION_ENHANCED
    fix += min(10.0, floor(fix * (127.0 - fix) * (0.32 / 127.0) + 0.5));
#endif
    return fix;
}

vec3 ps2EnvironmentReflection(vec3 base, vec3 environment, vec3 primary, float coefficient) {
    vec3 reflected = clamp(floor(environment * primary / 128.0), 0.0, 255.0);
    float fix = ps2EnvironmentFix(coefficient);
    return clamp(floor((reflected - base) * fix / 128.0) + base, 0.0, 255.0);
}

vec4 ps2TextureFunction(vec4 texel, vec4 fragment) {
    vec4 result = floor(texel * fragment / 128.0);
    if (uPs2TextureFunction == 1) result = texel;
    else if (uPs2TextureFunction == 2 || uPs2TextureFunction == 3) {
        result.rgb += fragment.a;
        result.a = uPs2TextureFunction == 2 ? texel.a + fragment.a : texel.a;
    }
    if (uPs2TextureAlpha == 0) result.a = fragment.a;
    return clamp(result, 0.0, 255.0);
}

void main() {
    vec4 primary = floor(clamp(vPs2Color / vPs2ClipW, 0.0, 255.0) + 0.0001);
    vec4 texel = vec4(128.0);
    if (uUseTexture != 0) {
        texel = floor(texture2D(tex0, vTex0) * 255.0 + 0.5);
        if (uPs2NativeTextureAlpha == 0) texel.a = floor(texel.a * (128.0 / 255.0) + 0.5);
    }
    if (uPrelightMode == 1) {
        vec4 inspection = clamp(vPs2InspectionColor / vPs2ClipW, 0.0, 1.0);
        if (uUseTexture != 0) inspection *= vec4(texel.rgb / 255.0, min(texel.a / 128.0, 1.0));
        gl_FragColor = inspection;
        return;
    }
    vec4 color = uUseTexture != 0 ? ps2TextureFunction(texel, primary) : primary;
#if STORYLAND_PS2_BUDOKAI_REFLECTION
    if (uPs2BudokaiReflection != 0 && (uDualTextureMode == 2 || uDualTextureMode == 4) &&
            uPrelightMode == 0 && (uRenderMode == 0 || uRenderMode == 2)) {
        vec2 environmentUv = uDualTextureMode == 4
            ? ps2BudokaiEnvironmentAtlasUv(reflect(-normalize(vPs2ReflectionView),
                normalize(vPs2ReflectionNormal)))
            : vNativeEnvironmentUV * uDualTextureScale.xy;
        vec4 environment = texture2D(tex1, environmentUv);
        float nativeAlphaScale = uDualTextureMode == 2 ? 255.0 / 128.0 : 1.0;
        color.rgb = ps2BudokaiEnvironmentReflection(color.rgb, environment,
            uDualTextureStrength, nativeAlphaScale);
    }
#endif
    if (uPs2Fog.z != 0.0) {
        float fog = floor(clamp(vPs2Fog / vPs2ClipW, 0.0, 255.0) + 0.0001);
        vec3 fogColor = floor(clamp(uFogColor, 0.0, 1.0) * 255.0 + 0.0001);
        color.rgb = floor(color.rgb * fog / 256.0) + floor(fogColor * (255.0 - fog) / 256.0);
    }
    if (uDualTextureMode == 2 && uPrelightMode == 0 && (uRenderMode == 0 || uRenderMode == 2)
#if STORYLAND_PS2_BUDOKAI_REFLECTION
            && uPs2BudokaiReflection == 0
#endif
            ) {
        vec4 environment = floor(texture2D(tex1, vNativeEnvironmentUV * uDualTextureScale.xy) * 255.0 + 0.5);
        color.rgb = ps2EnvironmentReflection(color.rgb, environment.rgb, primary.rgb, uPs2EnvironmentCoefficient);
    }
    gl_FragColor = vec4(color.rgb / 255.0, clamp(color.a / 128.0, 0.0, 1.0));
}
