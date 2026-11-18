#ifdef GL_ARB_shader_texture_lod
#extension GL_ARB_shader_texture_lod : enable
#endif

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform int uUseTexture;
uniform int uRenderMode;
uniform int uPrelightMode;
uniform vec3 uFogColor;
uniform float uPspTextureScale;
uniform vec2 uPspTextureSize;
uniform vec2 uPspEnvironmentSize;
uniform float uPspMaxMipLevel;
uniform float uPspTextureLodBias;
uniform int uPspTextureLodMode;
uniform float uPspTextureLodSlope;
uniform float uPspEnvironmentStrength;
uniform int uDualTextureUvSet;
uniform int uDualTextureMode;
uniform float uDualTextureStrength;
uniform vec3 uDualTextureScale;

varying vec4 vPspColor;
varying float vPspFog;
varying float vPspClipW;
varying vec2 vTex0;
varying vec2 vTex1;
varying vec2 vNativeEnvironmentUV;
varying vec3 vPspEnvironmentColor;

vec4 pspTexel(sampler2D raster, vec2 uv, float level) {
#if defined(GL_ES)
    return floor(textureLod(raster, uv, level) * 255.0 + 0.5);
#elif defined(GL_ARB_shader_texture_lod)
    return floor(texture2DLod(raster, uv, level) * 255.0 + 0.5);
#else
    return floor(texture2D(raster, uv) * 255.0 + 0.5);
#endif
}

vec4 pspBilinear(sampler2D raster, vec2 uv, float level, vec2 textureSize) {
    vec2 size = max(floor(textureSize / exp2(level)), vec2(1.0));
    vec2 position = uv * size * 16.0;
    position = sign(position) * floor(abs(position)) - 8.0;
    vec2 integerPosition = floor(position / 16.0);
    vec2 weight = position - integerPosition * 16.0;
    vec2 origin = (integerPosition + 0.5) / size;
    vec2 texel = 1.0 / size;
    vec4 topLeft = pspTexel(raster, origin, level);
    vec4 topRight = pspTexel(raster, origin + vec2(texel.x, 0.0), level);
    vec4 bottomLeft = pspTexel(raster, origin + vec2(0.0, texel.y), level);
    vec4 bottomRight = pspTexel(raster, origin + texel, level);
    vec4 top = floor((topLeft * (16.0 - weight.x) + topRight * weight.x) / 16.0);
    vec4 bottom = floor((bottomLeft * (16.0 - weight.x) + bottomRight * weight.x) / 16.0);
    return floor((top * (16.0 - weight.y) + bottom * weight.y) / 16.0);
}

float pspLog16(float value) {
    float exponent = floor(log2(max(value, 0.00000000000000000001)));
    float fraction = value / exp2(exponent) - 1.0;
    return exponent * 16.0 + floor(fraction * 16.0);
}

vec4 pspSampleTexture(vec2 uv) {
    if (uPspTextureSize.x <= 0.0 || uPspTextureSize.y <= 0.0)
        return floor(texture2D(tex0, uv) * 255.0 + 0.5);
#if !defined(GL_ES) && !defined(GL_ARB_shader_texture_lod)
    // A legacy driver without explicit fragment LOD retains hardware filtering.
    return floor(texture2D(tex0, uv) * 255.0 + 0.5);
#else
    float reciprocalW = 1.0 / vPspClipW;
    vec2 projectedUv = uv * reciprocalW * uPspTextureSize;
    vec2 horizontal = abs(dFdx(projectedUv));
    vec2 vertical = abs(dFdy(projectedUv));
    float gradient = max(max(horizontal.x, horizontal.y), max(vertical.x, vertical.y));
    float detail = pspLog16(gradient) - pspLog16(reciprocalW);
    if (uPspTextureLodMode == 1) detail = 0.0;
    else if (uPspTextureLodMode == 2)
        detail = 16.0 + pspLog16(uPspTextureLodSlope) - pspLog16(reciprocalW);
    detail = clamp(detail + floor(uPspTextureLodBias * 16.0), 0.0, uPspMaxMipLevel * 16.0);
    float level = floor(detail / 16.0);
    float fraction = detail - level * 16.0;
    vec4 first = pspBilinear(tex0, uv, level, uPspTextureSize);
    if (fraction == 0.0 || level >= uPspMaxMipLevel) return first;
    vec4 second = pspBilinear(tex0, uv, level + 1.0, uPspTextureSize);
    return floor((first * (16.0 - fraction) + second * fraction) / 16.0);
#endif
}

void main() {
    vec4 primary = floor(clamp(vPspColor / vPspClipW, 0.0, 255.0) + 0.0001);
    vec4 color = primary;
    if (uUseTexture != 0) {
        vec4 texel = pspSampleTexture(vTex0);
        float scale = uPrelightMode == 0 ? uPspTextureScale : 1.0;
        color.rgb = floor(texel.rgb * (primary.rgb + 1.0) * scale / 256.0);
        color.a = floor(texel.a * (primary.a + 1.0) / 256.0);
    }

    color = clamp(color, 0.0, 255.0);
    float fog = floor(clamp(vPspFog / vPspClipW, 0.0, 255.0) + 0.0001);
    vec3 fogColor = floor(clamp(uFogColor, 0.0, 1.0) * 255.0 + 0.0001);
    if (uPrelightMode != 1) {
        color.rgb = floor((color.rgb * fog + fogColor * (255.0 - fog) + 255.0) / 256.0);
    }
    if (uDualTextureMode == 2 && uPrelightMode == 0 && (uRenderMode == 0 || uRenderMode == 2)) {
        vec2 environmentUv = vNativeEnvironmentUV * uDualTextureScale.xy;
        vec4 texel = uPspEnvironmentSize.x > 0.0 && uPspEnvironmentSize.y > 0.0
            ? pspBilinear(tex1, environmentUv, 0.0, uPspEnvironmentSize)
            : floor(texture2D(tex1, environmentUv) * 255.0 + 0.5);
        vec3 primaryEnvironment = floor(clamp(vPspEnvironmentColor / vPspClipW,0.0,255.0) + 0.0001);
        vec3 environment = clamp(floor(2.0 * texel.rgb * (primaryEnvironment + 1.0) / 256.0),0.0,255.0);
        float amount = floor(clamp(uDualTextureStrength * 0.5 * uPspEnvironmentStrength,0.0,1.0) * 255.0);
        vec3 source = floor((2.0 * environment + 1.0) * (2.0 * amount + 1.0) / 1024.0);
        vec3 destination = floor((2.0 * color.rgb + 1.0) * (2.0 * (255.0 - amount) + 1.0) / 1024.0);
        color.rgb = clamp(source + destination,0.0,255.0);
    }
    gl_FragColor = color / 255.0;
}
