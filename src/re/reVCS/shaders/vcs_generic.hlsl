// VCS desktop shader. Native timecycle values are supplied by the host.
cbuffer StoriesFrame : register(b0)
{
    row_major float4x4 modelView;
    row_major float4x4 modelViewProjection;
    float4 ambientColor;
    float4 directionalColor;
    float4 fogColor;
    float fogStart;
    float farClip;
    int useTexture;
    int renderMode;
    int prelightMode;
    int worldGeometry;
    float alphaReference;
    int alphaTest;
    float4 sunDirection;
};
Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);
struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};
struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float3 lighting : TEXCOORD1;
    float fog : TEXCOORD2;
    float3 specular : TEXCOORD3;
    float3 materialReflection : TEXCOORD4;
};
VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    float4 position = float4(input.position, 1.0);
    float4 eye = mul(position, modelView);
    output.position = mul(position, modelViewProjection);
    output.color = input.color;
    output.uv = input.uv;

    float3 transformedNormal = mul(float4(input.normal, 0.0), modelView).xyz;
    float3 normal = dot(transformedNormal, transformedNormal) > 0.000001 ? normalize(transformedNormal) : float3(0.0, 0.0, 1.0);
    float3 lightDir = dot(sunDirection.xyz, sunDirection.xyz) > 0.000001 ?
        normalize(sunDirection.xyz) : float3(0.0, 0.0, 1.0);
    float distanceFromCamera = length(eye.xyz);
    float3 viewVector = -eye.xyz;
    float3 viewDir = dot(viewVector, viewVector) > 0.000001 ? normalize(viewVector) : float3(0.0, 0.0, 1.0);
    float ndl = max(dot(normal, lightDir), 0.0);
    float wrapped = saturate(ndl * 0.72 + 0.28);
    float shadowFill = 1.0 - ndl;
    float3 baseLight = ambientColor.rgb * 1.100
        + directionalColor.rgb * (wrapped * 0.620 + shadowFill * 0.100)
        + 0.055;
    output.lighting = clamp(baseLight, 0.220, 1.18);

    float3 halfVector = lightDir + viewDir;
    float3 halfDir = dot(halfVector, halfVector) > 0.000001 ? normalize(halfVector) : lightDir;
    float highlightFade = 1.0 / (1.0 + distanceFromCamera * 0.0013);
    float specular = pow(max(dot(normal, halfDir), 0.0), 16.0) * 0.015 * highlightFade;
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.5);
    float rim = fresnel * 0.020 * highlightFade;
    float lightFacing = saturate(ndl * 3.0);
    output.specular = directionalColor.rgb * (specular * lightFacing) + ambientColor.rgb * rim;

    float materialFacing = saturate(dot(normal, viewDir));
    float reflectedSun = max(dot(reflect(-viewDir, normal), lightDir), 0.0);
    output.materialReflection = (ambientColor.rgb * 0.30 + directionalColor.rgb *
        (0.10 + 0.80 * pow(reflectedSun, 12.0))) * (0.25 + 0.75 * pow(1.0 - materialFacing, 2.0));

    float fogLinear = saturate((farClip - distanceFromCamera) / max(farClip - fogStart, 0.001));
    output.fog = fogLinear * fogLinear * (3.0 - 2.0 * fogLinear);
    return output;
}
float4 PSMain(VertexOutput input) : SV_TARGET
{
    float4 texel = useTexture != 0 ? diffuseTexture.Sample(diffuseSampler, input.uv) : float4(1, 1, 1, 1);
    float alpha = texel.a * saturate(input.color.a);
    if (alphaTest != 0 && alpha <= alphaReference) discard;
    float3 vertexColor = saturate(input.color.rgb);
    float3 lit = texel.rgb;
    if (prelightMode == 1)
        lit = texel.rgb * vertexColor;
    else if (prelightMode == 2)
        lit = texel.rgb;
    else if (renderMode == 0 || renderMode == 2)
    {
        float3 authored = texel.rgb * vertexColor;
        lit = authored * lerp(float3(1, 1, 1), clamp(input.lighting, 0.0, 1.18), 0.200) + input.specular * lerp(float3(1, 1, 1), texel.rgb, 0.10) * texel.a;
    }
    else
        lit = texel.rgb * vertexColor;
    if (prelightMode == 0 && (renderMode == 0 || renderMode == 2))
        lit += input.materialReflection * saturate(sunDirection.w) * texel.a;
    if (prelightMode == 0 && (renderMode == 0 || renderMode == 2))
    {
        float luminance = dot(lit, float3(0.2126, 0.7152, 0.0722));
        lit = lerp(float3(luminance, luminance, luminance), lit, 1.010);
        float3 excess = max(lit - 1.0, 0.0);
        lit = lit / (1.0 + excess * 0.55);
    }
    if (prelightMode != 1)
        lit = lerp(fogColor.rgb, clamp(lit, 0.0, 1.12), input.fog);
    return float4(saturate(lit), alpha);
}
