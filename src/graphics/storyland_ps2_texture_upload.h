#pragma once

#include "src/leeds/leeds_texture.h"
#include "src/graphics/storyland_texture_upload.h"
#include <algorithm>
#include <stdexcept>
#include <vector>

inline std::vector<uint8_t> storylandPs2UploadPixels(const uint8_t* rgba, size_t bytes,
    const uint8_t* nativeAlpha, size_t alphaBytes, bool fiveBitColor) {
    std::vector<uint8_t> pixels;
    if (nativeAlpha && alphaBytes == bytes / 4u) {
        std::string error;
        if (!leedsBuildNativePs2Rgba(rgba, bytes, nativeAlpha, alphaBytes, fiveBitColor, pixels, error))
            throw std::runtime_error(error);
    } else {
        pixels.assign(rgba, rgba + bytes);
        for (size_t index = 3; index < pixels.size(); index += 4)
            pixels[index] = uint8_t((unsigned(pixels[index]) * 128u + 127u) / 255u);
    }
    return pixels;
}

template<class Image>
void storylandUploadStoriesTexture(const Image& image, bool ps2) {
    if (!ps2) {
        storylandUploadRgbaTexture(image);
        return;
    }
    const size_t count = storylandAuthoredMipCount(image.width, image.height, image.mipLevels);
    glTexParameteri(GL_TEXTURE_2D, StorylandTextureMaxLevel, GLint(count));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, count ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    auto pixels = storylandPs2UploadPixels(image.rgba.data(), image.rgba.size(),
        image.ps2Alpha.data(), image.ps2Alpha.size(), image.ps2FiveBitColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (size_t index = 0; index < count; ++index) {
        const auto& level = image.mipLevels[index];
        pixels = storylandPs2UploadPixels(level.rgba.data(), level.rgba.size(),
            level.ps2Alpha.data(), level.ps2Alpha.size(), level.ps2FiveBitColor);
        glTexImage2D(GL_TEXTURE_2D, GLint(index + 1), GL_RGBA, level.width, level.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
}
