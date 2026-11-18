#pragma once

#include "src/shaders/storyland_shaders.h"
#include <string>

inline constexpr const char* StorylandVcsPspHlsl=R"HLSL(
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
float4 textureInformation : register(c19);
float4 consoleAmbient : register(c20);
float4 consoleEmissive : register(c21);
float4 environmentS : register(c22);
float4 environmentT : register(c23);
float4 pspParameters : register(c24);
float4 extraLightEye0 : register(c25);
float4 extraLightColor0 : register(c26);
float4 extraLightEye1 : register(c27);
float4 extraLightColor1 : register(c28);
float4 environmentTextureInformation : register(c29);

#define fogStart frameParameters.x
#define farClip frameParameters.y
#define useTexture frameParameters.z
#define renderMode frameParameters.w
#define prelightMode surfaceParameters.x
#define alphaReference surfaceParameters.z
#define alphaTest surfaceParameters.w
#define textureSize textureInformation.xy
#define maximumMipLevel textureInformation.z
#define textureLodBias textureInformation.w
#define textureScale consoleAmbient.w
#define prelitModel consoleEmissive.w
#define textureLodMode pspParameters.x
#define textureLodSlope pspParameters.y
#define depthOffset pspParameters.z
#define environmentStrength pspParameters.w
#define environmentSize environmentTextureInformation.xy
#define pspFogSlope environmentTextureInformation.z

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
    float4 pspColor : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float2 uv1 : TEXCOORD2;
    float pspFog : TEXCOORD3;
    float clipW : TEXCOORD4;
    float2 environmentUv : TEXCOORD5;
    float3 environmentColor : TEXCOORD6;
};

float3 pspLightProduct(float3 light,float3 material) {
    return floor((2.0*light+1.0)*(2.0*material+1.0)/1024.0);
}

float3 pspLightScale(float3 color,float factor) {
    float amount=floor(saturate(factor)*256.0);
    return floor((2.0*color+1.0)*(2.0*amount+1.0)/1024.0);
}

float3 pspLightBytes(float3 color) {
    return floor(saturate(color)*255.0+0.0001);
}

float3 pspDirectionalLight(float3 normal,float3 direction,float3 color,float3 material) {
    float normalLengthSquared=dot(normal,normal);
    float lightLengthSquared=dot(direction,direction);
    float diffuse=0.0;
    if(normalLengthSquared>0.0&&lightLengthSquared>0.0)
        diffuse=max(dot(normal,direction)*rsqrt(normalLengthSquared*lightLengthSquared),0.0);
    return pspLightScale(pspLightProduct(pspLightBytes(color),material),diffuse);
}

VertexOutput VSMain(VertexInput input) {
    VertexOutput output;
    float4 position=float4(input.position,1.0);
    float4 eye=mul(position,modelView);
    output.position=mul(position,modelViewProjection);
    output.position.z+=depthOffset*0.5*output.position.w;
    output.uv=input.uv;
    output.uv1=input.uv1;
    float4 material=floor(saturate(input.color)*255.0+0.5);
    float3 primary=material.rgb;
    float3 normal=mul(float4(input.normal,0.0),modelView).xyz;
    float normalLengthSquared=dot(normal,normal);
    if(prelightMode==2.0) {
        primary=float3(255.0,255.0,255.0);
    } else if(prelightMode==0.0&&(renderMode==0.0||renderMode==2.0)) {
        if(prelitModel!=0.0) {
            primary=material.rgb+pspLightBytes(consoleEmissive.rgb);
        } else {
#if STORYLAND_PSP_WORLD
        primary=pspLightProduct(pspLightBytes(consoleAmbient.bgr),material.rgb)
            +pspLightBytes(consoleEmissive.rgb);
#else
        primary=pspLightProduct(pspLightBytes(ambientColor.rgb),material.rgb)
            +pspDirectionalLight(normal,lightEye.xyz,directionalColor.rgb,material.rgb)
            +pspDirectionalLight(normal,extraLightEye0.xyz,extraLightColor0.rgb,material.rgb)
            +pspDirectionalLight(normal,extraLightEye1.xyz,extraLightColor1.rgb,material.rgb);
#endif
        }
    }
    float3 environmentNormal=normalLengthSquared>0.0?normal*rsqrt(normalLengthSquared):float3(0,0,0);
    float2 environmentDots=float2(dot(environmentNormal,environmentS.xyz),dot(environmentNormal,environmentT.xyz));
    output.environmentUv=environmentDots*0.5+0.5;
    float3 environmentColor=pspLightBytes(ambientColor.rgb)
        +pspLightScale(float3(255,255,255),environmentDots.x)
        +pspLightScale(float3(255,255,255),environmentDots.y);
    output.environmentColor=clamp(environmentColor,0.0,255.0)*output.position.w;
    float fogSlope=pspFogSlope>0.0?pspFogSlope:1.0/max(farClip-fogStart,0.000001);
    float fog=saturate((farClip+eye.z)*fogSlope);
    float fogByte=min(floor(fog*256.0),255.0);
    output.clipW=output.position.w;
    output.pspColor=float4(clamp(primary,0.0,255.0),material.a)*output.position.w;
    output.pspFog=fogByte*output.position.w;
    return output;
}

float4 pspTexel(sampler2D raster,float2 uv,float level) {
    return floor(tex2Dlod(raster,float4(uv,0.0,level))*255.0+0.5);
}

float4 pspBilinear(sampler2D raster,float2 uv,float level,float2 rasterSize) {
    float2 size=max(floor(rasterSize/exp2(level)),float2(1.0,1.0));
    float2 position=uv*size*16.0;
    position=sign(position)*floor(abs(position))-8.0;
    float2 integerPosition=floor(position/16.0);
    float2 weight=position-integerPosition*16.0;
    float2 origin=(integerPosition+0.5)/size;
    float2 texel=1.0/size;
    float4 topLeft=pspTexel(raster,origin,level);
    float4 topRight=pspTexel(raster,origin+float2(texel.x,0.0),level);
    float4 bottomLeft=pspTexel(raster,origin+float2(0.0,texel.y),level);
    float4 bottomRight=pspTexel(raster,origin+texel,level);
    float4 top=floor((topLeft*(16.0-weight.x)+topRight*weight.x)/16.0);
    float4 bottom=floor((bottomLeft*(16.0-weight.x)+bottomRight*weight.x)/16.0);
    return floor((top*(16.0-weight.y)+bottom*weight.y)/16.0);
}

float pspLog16(float value) {
    float exponent=floor(log2(max(value,0.00000000000000000001)));
    float fraction=value/exp2(exponent)-1.0;
    return exponent*16.0+floor(fraction*16.0);
}

float4 pspSampleTexture(float2 uv,float clipW) {
    if(textureSize.x<=0.0||textureSize.y<=0.0)
        return floor(tex2D(diffuseSampler,uv)*255.0+0.5);
    float reciprocalW=1.0/clipW;
    float2 projectedUv=uv*reciprocalW*textureSize;
    float2 horizontal=abs(ddx(projectedUv));
    float2 vertical=abs(ddy(projectedUv));
    float gradient=max(max(horizontal.x,horizontal.y),max(vertical.x,vertical.y));
    float detail=pspLog16(gradient)-pspLog16(reciprocalW);
    if(textureLodMode==1.0)detail=0.0;
    else if(textureLodMode==2.0)
        detail=16.0+pspLog16(textureLodSlope)-pspLog16(reciprocalW);
    detail=clamp(detail+floor(textureLodBias*16.0),0.0,maximumMipLevel*16.0);
    float level=floor(detail/16.0);
    float fraction=detail-level*16.0;
    float4 first=pspBilinear(diffuseSampler,uv,level,textureSize);
    if(fraction==0.0||level>=maximumMipLevel)return first;
    float4 second=pspBilinear(diffuseSampler,uv,level+1.0,textureSize);
    return floor((first*(16.0-fraction)+second*fraction)/16.0);
}

float4 PSMain(VertexOutput input) : COLOR0 {
    float4 primary=floor(clamp(input.pspColor/input.clipW,0.0,255.0)+0.0001);
    float4 color=primary;
    if(useTexture!=0.0) {
        float4 texel=pspSampleTexture(input.uv,input.clipW);
        float scale=prelightMode==0.0?textureScale:1.0;
        color.rgb=floor(texel.rgb*(primary.rgb+1.0)*scale/256.0);
        color.a=floor(texel.a*(primary.a+1.0)/256.0);
    }
    color=clamp(color,0.0,255.0);
    float fog=floor(clamp(input.pspFog/input.clipW,0.0,255.0)+0.0001);
    float3 fogBytes=pspLightBytes(fogColor.rgb);
    if(prelightMode!=1.0) {
        color.rgb=floor((color.rgb*fog+fogBytes*(255.0-fog)+255.0)/256.0);
    }
    if(dualParameters.x==2.0&&prelightMode==0.0&&(renderMode==0.0||renderMode==2.0)) {
        float2 environmentUv=input.environmentUv*dualParameters.zw;
        float4 texel=environmentSize.x>0.0&&environmentSize.y>0.0?
            pspBilinear(environmentSampler,environmentUv,0.0,environmentSize):
            floor(tex2D(environmentSampler,environmentUv)*255.0+0.5);
        float3 primaryEnvironment=floor(clamp(input.environmentColor/input.clipW,0.0,255.0)+0.0001);
        float3 environment=clamp(floor(2.0*texel.rgb*(primaryEnvironment+1.0)/256.0),0.0,255.0);
        float amount=floor(saturate(dualParameters.y*0.5*environmentStrength)*255.0);
        float3 source=floor((2.0*environment+1.0)*(2.0*amount+1.0)/1024.0);
        float3 destination=floor((2.0*color.rgb+1.0)*(2.0*(255.0-amount)+1.0)/1024.0);
        color.rgb=clamp(source+destination,0.0,255.0);
    }
    return color/255.0;
}
)HLSL";

inline std::string storylandVcsPspShaderSource(StorylandShaderKind kind) {
    const bool prelit=kind==StorylandShaderKind::World||kind==StorylandShaderKind::Vegetation;
    return std::string("#define STORYLAND_PSP_WORLD ")+(prelit?"1\n":"0\n")+StorylandVcsPspHlsl;
}
