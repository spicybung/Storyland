#pragma once

#include "src/shaders/storyland_shaders.h"
#include <string>

inline constexpr const char* StorylandPs2Hlsl = R"HLSL(
#ifndef STORYLAND_PS2_REFLECTION_ENHANCED
#define STORYLAND_PS2_REFLECTION_ENHANCED 0
#endif

#ifndef STORYLAND_PS2_BUDOKAI_REFLECTION
#define STORYLAND_PS2_BUDOKAI_REFLECTION 0
#endif

row_major float4x4 modelView : register(c0);
row_major float4x4 modelViewProjection : register(c4);
float4 ambientColor : register(c8);
float4 directionalColor : register(c9);
float4 fogColor : register(c10);
float4 frameParameters : register(c11);
float4 surfaceParameters : register(c12);
float4 lightEye : register(c13);
float4 dualParameters : register(c14);
float4 environmentParameters : register(c15);
#if STORYLAND_PS2_BUDOKAI_REFLECTION
float4 environmentViewRight : register(c16);
float4 environmentViewUp : register(c17);
float4 environmentViewBack : register(c18);
#endif
float4 worldMultiplier : register(c19);
float4 worldAdditive : register(c20);
float4 colorCeiling : register(c21);
float4 actorAmbient : register(c22);
float4 actorDirectional : register(c23);
float4 fogParameters : register(c24);
float4 environmentS : register(c25);
float4 environmentT : register(c26);
float4 extraLightEye0 : register(c27);
float4 extraLightColor0 : register(c28);
float4 extraLightEye1 : register(c29);
float4 extraLightColor1 : register(c30);
float4 extraLightEye2 : register(c31);
float4 extraLightColor2 : register(c32);
float4 textureParameters : register(c33);
float4 reflectionParameters : register(c34);

#define useTexture frameParameters.z
#define renderMode frameParameters.w
#define prelightMode surfaceParameters.x
#define textureFunction textureParameters.x
#define textureAlpha textureParameters.y
#define nativeTextureAlpha textureParameters.z
#define prelitModel textureParameters.w

sampler2D diffuseSampler : register(s0);
sampler2D environmentSampler : register(s1);

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct VertexOutput {
    float4 position : POSITION;
    float4 ps2Color : TEXCOORD0;
    float4 inspectionColor : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float2 uv1 : TEXCOORD3;
    float2 fogAndClipW : TEXCOORD4;
    float2 environmentUv : TEXCOORD5;
#if STORYLAND_PS2_BUDOKAI_REFLECTION
    float3 reflectionNormal : TEXCOORD6;
    float3 reflectionView : TEXCOORD7;
#endif
};

#if STORYLAND_PS2_BUDOKAI_REFLECTION
float3 ps2BudokaiNormal(float3 sourceNormal) {
    float3 axisX = modelView[0].xyz;
    float3 axisY = modelView[1].xyz;
    float3 axisZ = modelView[2].xyz;
    float3 cofactorX = cross(axisY, axisZ);
    float3 normal = cofactorX * sourceNormal.x + cross(axisZ, axisX) * sourceNormal.y +
        cross(axisX, axisY) * sourceNormal.z;
    if (dot(axisX, cofactorX) < 0.0) normal = -normal;
    float normalLength = length(normal);
    return normalLength > 0.0 ? normal / normalLength : float3(0.0, 0.0, 1.0);
}
#endif

VertexOutput VSMain(VertexInput input) {
    VertexOutput output;
    float4 position = float4(input.position, 1.0);
    float4 eyePosition = mul(position, modelView);
    output.position = mul(position, modelViewProjection);
    float4 material = floor(saturate(input.color) * 255.0 + 0.5);
    float4 primary = material;
    float3 axisX = modelView[0].xyz;
    float3 axisY = modelView[1].xyz;
    float3 axisZ = modelView[2].xyz;
    axisX /= max(length(axisX), 0.000001);
    axisY /= max(length(axisY), 0.000001);
    axisZ /= max(length(axisZ), 0.000001);
    float3 normal = axisX * input.normal.x + axisY * input.normal.y + axisZ * input.normal.z;

#if STORYLAND_PS2_WORLD
    primary = floor(clamp(material * worldMultiplier + worldAdditive, 0.0, colorCeiling));
#else
    if (prelitModel != 0.0) {
        primary = floor(clamp(material * worldMultiplier + worldAdditive, 0.0, colorCeiling));
    } else {
        float3 materialScale = material.rgb * (1.0 / 510.0);
        float3 color = (actorDirectional.rgb * materialScale) * max(dot(normal, lightEye.xyz), 0.0);
        color += (extraLightColor0.rgb * materialScale) * max(dot(normal, extraLightEye0.xyz), 0.0);
        color += (extraLightColor1.rgb * materialScale) * max(dot(normal, extraLightEye1.xyz), 0.0);
        color += (extraLightColor2.rgb * materialScale) * max(dot(normal, extraLightEye2.xyz), 0.0);
        color += actorAmbient.rgb * materialScale;
        primary.rgb = floor(clamp(color, 0.0, colorCeiling.rgb));
        primary.a = floor((material.a + 1.0) * 0.5);
    }
#endif

    if (prelightMode == 2.0 || renderMode == 1.0 || renderMode == 3.0)
        primary.rgb = float3(128.0, 128.0, 128.0);
    output.ps2Color = primary * output.position.w;
    output.inspectionColor = input.color * output.position.w;
    output.uv = input.uv;
    output.uv1 = input.uv1;
    output.environmentUv = float2(0.5, 0.5) +
        float2(dot(normal, environmentS.xyz), dot(normal, environmentT.xyz)) * 0.5;
#if STORYLAND_PS2_BUDOKAI_REFLECTION
    output.reflectionNormal = ps2BudokaiNormal(input.normal);
    output.reflectionView = normalize(-eyePosition.xyz + float3(0.00001, 0.00001, 0.00001));
    if (reflectionParameters.y != 0.0) {
        float3 reflected = reflect(-output.reflectionView, output.reflectionNormal);
        output.environmentUv = reflected.xy * 0.5 + float2(0.5, 0.5);
    }
#endif
    float depth = -eyePosition.z;
    float fogFixed = floor(clamp(depth * fogParameters.x + fogParameters.y, 0.0, 255.0) * 16.0);
    output.fogAndClipW = float2(floor(fogFixed / 16.0), 1.0) * output.position.w;
    return output;
}

#if STORYLAND_PS2_BUDOKAI_REFLECTION
float2 ps2BudokaiEnvironmentAtlasUv(float3 eyeDirection) {
    float3 direction = environmentViewRight.xyz * eyeDirection.x +
        environmentViewUp.xyz * eyeDirection.y + environmentViewBack.xyz * eyeDirection.z;
    float3 magnitude = abs(direction);
    float2 faceUv;
    float face;
    if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z) {
        face = direction.x >= 0.0 ? 0.0 : 1.0;
        faceUv = float2(direction.x >= 0.0 ? -direction.z : direction.z, -direction.y)
            / max(magnitude.x, 0.00001);
    } else if (magnitude.y >= magnitude.z) {
        face = direction.y >= 0.0 ? 2.0 : 3.0;
        faceUv = float2(direction.x, direction.y >= 0.0 ? direction.z : -direction.z)
            / max(magnitude.y, 0.00001);
    } else {
        face = direction.z >= 0.0 ? 4.0 : 5.0;
        faceUv = float2(direction.z >= 0.0 ? direction.x : -direction.x, -direction.y)
            / max(magnitude.z, 0.00001);
    }
    float inset = 0.5 / max(environmentParameters.y, 1.0);
    faceUv = clamp(faceUv * 0.5 + float2(0.5, 0.5), inset, 1.0 - inset);
    float2 uv = (float2(fmod(face, 3.0), floor(face / 3.0)) + faceUv) / float2(3.0, 2.0);
    uv.y = 1.0 - uv.y;
    return uv;
}

float3 ps2BudokaiEnvironmentReflection(float3 base, float4 environment,
        float strength, float nativeAlphaScale) {
    float alpha = clamp(environment.a * nativeAlphaScale, 0.0, 1.0);
    float3 reflected = environment.rgb * alpha * clamp(strength, 0.0, 1.0);
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

float3 ps2EnvironmentReflection(float3 base, float3 environment, float3 primary, float coefficient) {
    float3 reflected = clamp(floor(environment * primary / 128.0), 0.0, 255.0);
    float fix = ps2EnvironmentFix(coefficient);
    return clamp(floor((reflected - base) * fix / 128.0) + base, 0.0, 255.0);
}

float4 ps2TextureFunction(float4 texel, float4 fragment) {
    float4 result = floor(texel * fragment / 128.0);
    if (textureFunction == 1.0) result = texel;
    else if (textureFunction == 2.0 || textureFunction == 3.0) {
        result.rgb += fragment.a;
        result.a = textureFunction == 2.0 ? texel.a + fragment.a : texel.a;
    }
    if (textureAlpha == 0.0) result.a = fragment.a;
    return clamp(result, 0.0, 255.0);
}

float4 PSMain(VertexOutput input) : COLOR0 {
    float clipW = input.fogAndClipW.y;
    float4 primary = floor(clamp(input.ps2Color / clipW, 0.0, 255.0) + 0.0001);
    float4 texel = float4(128.0, 128.0, 128.0, 128.0);
    if (useTexture != 0.0) {
        texel = floor(tex2D(diffuseSampler, input.uv) * 255.0 + 0.5);
        if (nativeTextureAlpha == 0.0) texel.a = floor(texel.a * (128.0 / 255.0) + 0.5);
    }
    if (prelightMode == 1.0) {
        float4 inspection = saturate(input.inspectionColor / clipW);
        if (useTexture != 0.0) inspection *= float4(texel.rgb / 255.0, min(texel.a / 128.0, 1.0));
        return inspection;
    }
    float4 color = useTexture != 0.0 ? ps2TextureFunction(texel, primary) : primary;
#if STORYLAND_PS2_BUDOKAI_REFLECTION
    if (reflectionParameters.y != 0.0 && (dualParameters.x == 2.0 || dualParameters.x == 4.0) &&
            prelightMode == 0.0 && (renderMode == 0.0 || renderMode == 2.0)) {
        float2 environmentUv = dualParameters.x == 4.0
            ? ps2BudokaiEnvironmentAtlasUv(reflect(-normalize(input.reflectionView),
                normalize(input.reflectionNormal)))
            : input.environmentUv * dualParameters.zw;
        float4 environment = tex2D(environmentSampler, environmentUv);
        float nativeAlphaScale = dualParameters.x == 2.0 ? 255.0 / 128.0 : 1.0;
        color.rgb = ps2BudokaiEnvironmentReflection(color.rgb, environment,
            dualParameters.y, nativeAlphaScale);
    }
#endif
    if (fogParameters.z != 0.0) {
        float fog = floor(clamp(input.fogAndClipW.x / clipW, 0.0, 255.0) + 0.0001);
        float3 fogBytes = floor(saturate(fogColor.rgb) * 255.0 + 0.0001);
        color.rgb = floor(color.rgb * fog / 256.0) + floor(fogBytes * (255.0 - fog) / 256.0);
    }
    if (dualParameters.x == 2.0 && prelightMode == 0.0 && (renderMode == 0.0 || renderMode == 2.0)
#if STORYLAND_PS2_BUDOKAI_REFLECTION
            && reflectionParameters.y == 0.0
#endif
            ) {
        float4 environment = floor(tex2D(environmentSampler, input.environmentUv * dualParameters.zw) * 255.0 + 0.5);
        color.rgb = ps2EnvironmentReflection(color.rgb, environment.rgb, primary.rgb, reflectionParameters.x);
    }
    return float4(color.rgb / 255.0, saturate(color.a / 128.0));
}
)HLSL";

inline std::string storylandPs2ShaderSource(bool vcs, StorylandShaderKind kind) {
    const bool world = kind == StorylandShaderKind::World || kind == StorylandShaderKind::Vegetation;
    const bool enhancedReflection = kind == StorylandShaderKind::Vehicle || kind == StorylandShaderKind::Generic;
    const bool budokaiReflection = vcs && enhancedReflection;
    return std::string("#define STORYLAND_PS2_WORLD ") + (world ? "1\n" : "0\n") +
        "#define STORYLAND_PS2_REFLECTION_ENHANCED " + (enhancedReflection ? "1\n" : "0\n") +
        "#define STORYLAND_PS2_BUDOKAI_REFLECTION " + (budokaiReflection ? "1\n" : "0\n") +
        "#define STORYLAND_PS2_VCS " + (vcs ? "1\n" : "0\n") + StorylandPs2Hlsl;
}
