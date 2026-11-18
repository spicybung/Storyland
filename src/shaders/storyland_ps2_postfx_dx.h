#pragma once

inline constexpr const char* StorylandPs2PostEffectsFragmentDx = R"HLSL(
sampler2D sourceTexture : register(s0);
sampler2D originalTexture : register(s1);
float4 operationParameters : register(c0);
float4 filterColor : register(c1);
float4 captureUvScaleBias : register(c2);
float4 captureUvBounds : register(c3);

float3 bytesAt(float2 position) {
    return floor(tex2Dlod(sourceTexture,float4(position,0.0,0.0)).rgb*255.0+0.0001);
}

float4 PSMain(float2 uv : TEXCOORD0) : COLOR0 {
    float2 captureUv=clamp(uv*captureUvScaleBias.xy+captureUvScaleBias.zw,captureUvBounds.xy,captureUvBounds.zw);
    float4 originalSample=tex2Dlod(originalTexture,float4(captureUv,0.0,0.0));
    float3 original=floor(originalSample.rgb*255.0+0.0001);
    float3 color;
    float operation=operationParameters.x;
    if(operation==0.0) {
        color=max(bytesAt(captureUv)-floor(255.0*operationParameters.y/128.0),0.0);
    } else if(operation==1.0 || operation==2.0) {
        float2 stepSize=float2(1.0/256.0,1.0/256.0);
        float2 a=operation==1.0?float2(-1.0,0.0):float2(-1.0,-1.0);
        float2 b=operation==1.0?float2(1.0,0.0):float2(1.0,-1.0);
        float2 c=operation==1.0?float2(0.0,-1.0):float2(-1.0,1.0);
        float2 d=operation==1.0?float2(0.0,1.0):float2(1.0,1.0);
        color=floor(bytesAt(uv+a*stepSize)*(19.0/128.0))
             +floor(bytesAt(uv+b*stepSize)*(19.0/128.0))
             +floor(bytesAt(uv+c*stepSize)*(19.0/128.0))
             +floor(bytesAt(uv+d*stepSize)*(19.0/128.0));
    } else if(operation==3.0) {
        color=original+floor(bytesAt(uv)*(operationParameters.y/128.0));
    } else if(operation==4.0) {
        color=original;
        float2 shifted=float2((uv.x-operationParameters.y)/(1.0-operationParameters.y),uv.y);
        if(uv.x>=operationParameters.y)
            color+=floor((floor(bytesAt(shifted)*(127.0/128.0))-color)*(operationParameters.w/128.0));
        shifted.y=(uv.y-operationParameters.z)/(1.0-operationParameters.z);
        if(uv.x>=operationParameters.y && uv.y>=operationParameters.z)
            color+=floor((floor(bytesAt(shifted)*(127.0/128.0))-color)*(operationParameters.w/128.0));
        shifted.x=uv.x;
        if(uv.y>=operationParameters.z)
            color+=floor((floor(bytesAt(shifted)*(127.0/128.0))-color)*(operationParameters.w/128.0));
        color+=floor(bytesAt(uv)*filterColor.xyz/128.0);
    } else {
        color=floor(bytesAt(captureUv)*(127.0/128.0));
    }
    return float4(clamp(color,0.0,255.0)/255.0,originalSample.a);
}
)HLSL";
