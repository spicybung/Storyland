#pragma once

inline constexpr const char* StorylandPs2EnvironmentVertexGl = R"GLSL(
#version 120
void main() { gl_Position = gl_Vertex; }
)GLSL";

inline constexpr const char* StorylandPs2EnvironmentFragmentGl = R"GLSL(
#version 120
uniform sampler2D framebufferTexture;
uniform sampler2D coronaTexture;
uniform vec4 imageSizes;
uniform vec4 coronaRects[4];
uniform vec4 ditherRows[4];

vec4 pixelAt(sampler2D image, vec2 point, vec2 size, bool bottomUp, bool repeatImage) {
    point = repeatImage ? mod(point, size) : clamp(point, vec2(0.0), size - vec2(1.0));
    if (bottomUp) point.y = size.y - 1.0 - point.y;
    return floor(texture2D(image, (point + vec2(0.5)) / size) * 255.0 + vec4(0.5));
}

vec4 filteredAt(sampler2D image, vec2 coordinate, vec2 size, bool bottomUp, bool repeatImage) {
    vec2 samplePoint = coordinate - vec2(0.5);
    vec2 corner = floor(samplePoint);
    vec2 weight = floor((samplePoint - corner) * 16.0);
    vec4 a = pixelAt(image, corner, size, bottomUp, repeatImage);
    vec4 b = pixelAt(image, corner + vec2(1.0, 0.0), size, bottomUp, repeatImage);
    vec4 c = pixelAt(image, corner + vec2(0.0, 1.0), size, bottomUp, repeatImage);
    vec4 d = pixelAt(image, corner + vec2(1.0, 1.0), size, bottomUp, repeatImage);
    return floor((a * (16.0-weight.x) * (16.0-weight.y) +
        b * weight.x * (16.0-weight.y) + c * (16.0-weight.x) * weight.y +
        d * weight.x * weight.y) / 256.0);
}

vec4 quantize5551(vec4 color, float dither) {
    return vec4(floor(clamp(color.rgb + vec3(dither), 0.0, 255.0) / 8.0) * 8.0,
        floor(clamp(color.a, 0.0, 255.0) / 128.0) * 128.0);
}

void main() {
    vec2 point = floor(gl_FragCoord.xy);
    float dither = ditherRows[int(mod(point.y, 4.0))][int(mod(point.x, 4.0))];
    vec4 color = filteredAt(framebufferTexture, point * imageSizes.xy / 128.0,
        imageSizes.xy, true, false);
    color.a = 128.0;
    color = quantize5551(color, dither);
    for (int index = 0; index < 4; ++index) {
        vec4 rect = coronaRects[index];
        if (point.x >= rect.x && point.y >= rect.y && point.x < rect.z && point.y < rect.w) {
            vec2 coordinate = (point - rect.xy) * imageSizes.zw / (rect.zw - rect.xy);
            vec4 texel = filteredAt(coronaTexture, coordinate, imageSizes.zw, false, true);
            vec4 source = floor(texel * 127.0 / 128.0);
            color = quantize5551(vec4(color.rgb + floor(source.rgb * 255.0 / 128.0), source.a), dither);
        }
    }
    gl_FragColor = color / 255.0;
}
)GLSL";
