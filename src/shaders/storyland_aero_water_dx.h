#pragma once

inline constexpr const char* StorylandAeroWaterHlsl=R"HLSL(
row_major float4x4 modelView : register(c0);
row_major float4x4 modelViewProjection : register(c4);
float4 frame : register(c14);
float4 skyHorizonFogStart : register(c15);
float4 skyZenithFogEnd : register(c16);
float4 skyLowerReaction : register(c17);
float4 sunDirection : register(c18);
float4 sunColorTexture : register(c19);
float4 fogColor : register(c20);
float4 cameraPositionSize : register(c21);
float4 reactionRegion : register(c22);
float4 reactionTexel : register(c23);
sampler2D waterTexture : register(s0);
sampler2D reactionTexture : register(s1);

struct WaterInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};
struct WaterOutput {
    float4 position : POSITION;
    float3 world : TEXCOORD0;
    float3 eye : TEXCOORD1;
};
float waveHeight(float2 p) {
    float wind=saturate(frame.y),t=frame.x;
    return (.035+.080*wind)*sin(dot(p,float2(.095,.034))+t*.72)
         + (.026+.048*wind)*sin(dot(p,float2(-.041,.108))+t*.91+1.7)
         + (.070+.070*wind)*sin(dot(p,float2(.018,.011))+t*.31+.9)
         + (.055+.050*wind)*sin(dot(p,float2(-.013,.021))+t*.24+2.6);
}
float2 waveSlope(float2 p) {
    float wind=saturate(frame.y),t=frame.x;
    float2 s=(.035+.080*wind)*float2(.095,.034)*cos(dot(p,float2(.095,.034))+t*.72);
    s+=(.026+.048*wind)*float2(-.041,.108)*cos(dot(p,float2(-.041,.108))+t*.91+1.7);
    s+=(.070+.070*wind)*float2(.018,.011)*cos(dot(p,float2(.018,.011))+t*.31+.9);
    s+=(.055+.050*wind)*float2(-.013,.021)*cos(dot(p,float2(-.013,.021))+t*.24+2.6);
    return s;
}
WaterOutput VSMain(WaterInput input) {
    WaterOutput output;
    float4 world=float4(input.position,1.0);
    world.z+=waveHeight(world.xy);
    output.world=world.xyz;
    output.eye=mul(world,modelView).xyz;
    output.position=mul(world,modelViewProjection);
    return output;
}
float3 reactionValues(float2 p) {
    if(skyLowerReaction.w<.5)return float3(0,0,0);
    float2 uv=(p-reactionRegion.xy)*reactionRegion.z;
    if(uv.x<=.015||uv.y<=.015||uv.x>=.985||uv.y>=.985)return float3(0,0,0);
    float center=tex2D(reactionTexture,uv).r;
    float dx=tex2D(reactionTexture,uv+float2(reactionTexel.x,0)).r
           -tex2D(reactionTexture,uv-float2(reactionTexel.x,0)).r;
    float dy=tex2D(reactionTexture,uv+float2(0,reactionTexel.y)).r
           -tex2D(reactionTexture,uv-float2(0,reactionTexel.y)).r;
    return float3(center,dx,dy);
}
float4 PSMain(WaterOutput input) : COLOR0 {
    float2 p=input.world.xy;
    float wind=saturate(frame.y),rain=saturate(frame.z),t=frame.x;
    float2 slope=waveSlope(p);
    slope+=float2(.034*cos(dot(p,float2(.37,.18))+t*1.7),
                  .028*cos(dot(p,float2(-.17,.44))-t*1.31))*(.28+.72*wind);
    slope+=float2(.015*cos(dot(p,float2(.93,-.71))+t*2.2),
                  .019*cos(dot(p,float2(-.68,-.95))+t*1.8))*(.25+.75*wind);
    slope+=rain*.012*float2(cos(dot(p,float2(1.73,-1.29))+t*5.2),
                             cos(dot(p,float2(-2.01,-1.54))-t*4.6));
    float3 reaction=reactionValues(p);
    slope+=reaction.yz*.65;
    float3 n=normalize(float3(-slope,1.0));
    float3 v=normalize(cameraPositionSize.xyz-input.world);
    if(v.z<0)n=-n;
    float fresnel=.02+.98*pow(1-saturate(dot(n,v)),5);
    float3 reflection=reflect(-v,n);
    float3 sky=lerp(skyHorizonFogStart.rgb,skyZenithFogEnd.rgb,smoothstep(0.0,1.0,reflection.z));
    float3 lower=lerp(skyHorizonFogStart.rgb,skyLowerReaction.rgb,smoothstep(0.0,.65,-reflection.z));
    float3 environment=lerp(lower,sky,smoothstep(-.12,.14,reflection.z));
    float3 detail=float3(.5,.5,.5);
    if(sunColorTexture.w>.5) {
        detail=tex2D(waterTexture,p*.028+t*float2(.0042,.0029)).rgb*.48;
        detail+=tex2D(waterTexture,float2(dot(p,float2(.0211,.0174)),dot(p,float2(-.0174,.0211)))
                      +t*float2(-.0025,.0036)).rgb*.32;
        detail+=tex2D(waterTexture,float2(dot(p,float2(.0487,-.0196)),dot(p,float2(.0196,.0487)))
                      +t*float2(.0017,-.0021)).rgb*.20;
    }
    float3 ambient=max(.055*skyZenithFogEnd.rgb+.085*skyHorizonFogStart.rgb,float3(.01,.01,.01));
    float3 body=ambient*float3(.50,1.02,.88)*(.7+dot(detail,float3(.16,.28,.10)));
    body=lerp(body,skyLowerReaction.rgb*.23,saturate(rain*.36));
    float3 color=lerp(body,environment,saturate(.12+fresnel*.85));
    float3 light=normalize(sunDirection.xyz);
    float3 halfDirection=normalize(light+v);
    float ndh=saturate(dot(n,halfDirection));
    float sharp=pow(ndh,lerp(240.0,65.0,max(wind,rain)));
    float broad=pow(ndh,lerp(46.0,18.0,max(wind,rain)));
    color+=sunColorTexture.rgb*(sharp*1.22+broad*.14)*saturate(light.z)*frame.w;
    color+=reaction.x*.075*lerp(float3(.22,.42,.45),skyHorizonFogStart.rgb,.45);
    float fog=smoothstep(skyHorizonFogStart.w,max(skyHorizonFogStart.w+.001,skyZenithFogEnd.w),length(input.eye));
    color=lerp(color,fogColor.rgb,fog);
    return float4(saturate(color),saturate(.84+fresnel*.13));
}
)HLSL";
