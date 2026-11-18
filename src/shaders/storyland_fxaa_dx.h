#pragma once

inline const char* storylandFxaaDxShader() {
    return R"HLSL(
sampler2D scene : register(s0);
float4 sourceTexel : register(c0);
float4 sourceTransform : register(c1);
float4 sourceBounds : register(c2);
float4 sampleScene(float2 uv) {
    return tex2D(scene, clamp(uv, sourceBounds.xy, sourceBounds.zw));
}
float4 main(float2 coordinate : TEXCOORD0) : COLOR0 {
    float2 uv = coordinate * sourceTransform.xy + sourceTransform.zw;
    float3 weights = float3(.299, .587, .114);
    float4 center = sampleScene(uv);
    float3 northwest = sampleScene(uv + float2(-1, -1) * sourceTexel.xy).rgb;
    float3 northeast = sampleScene(uv + float2( 1, -1) * sourceTexel.xy).rgb;
    float3 southwest = sampleScene(uv + float2(-1,  1) * sourceTexel.xy).rgb;
    float3 southeast = sampleScene(uv + float2( 1,  1) * sourceTexel.xy).rgb;
    float middle = dot(center.rgb, weights);
    float nw = dot(northwest, weights), ne = dot(northeast, weights);
    float sw = dot(southwest, weights), se = dot(southeast, weights);
    float low = min(middle, min(min(nw, ne), min(sw, se)));
    float high = max(middle, max(max(nw, ne), max(sw, se)));
    if (high - low < max(1.0 / 32.0, high / 8.0)) return center;
    float2 direction = float2(-((nw + ne) - (sw + se)), (nw + sw) - (ne + se));
    float reduce = max((nw + ne + sw + se) / 32.0, 1.0 / 128.0);
    direction /= min(abs(direction.x), abs(direction.y)) + reduce;
    direction = clamp(direction, -8.0, 8.0) * sourceTexel.xy;
    float3 a = .5 * (sampleScene(uv - direction / 6.0).rgb + sampleScene(uv + direction / 6.0).rgb);
    float3 b = .5 * a + .25 * (sampleScene(uv - direction * .5).rgb + sampleScene(uv + direction * .5).rgb);
    float value = dot(b, weights);
    return float4(value < low || value > high ? a : b, center.a);
}
)HLSL";
}
