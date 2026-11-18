#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
namespace storyland_aero {
// Ported from our reLCS v6.9.45 HeightToNormalCache_d3d9.cpp.
static const float kDefaultNormalStrength = 1.8f;
static const float kStructuralBlurMix = 0.88f;
static const float kFineHeightWeight = 0.10f;
static const float kFineNoiseStrength = 0.015f;
static const float kMediumNoiseStrength = 0.0075f;

static float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static int clamp_index(int value, int limit)
{
    if (value < 0)
        return 0;
    if (value >= limit)
        return limit - 1;
    return value;
}

static uint32_t fnv1a_hash(const char *text)
{
    uint32_t hash = 2166136261u;
    if (!text)
        return hash;

    while (*text)
    {
        hash ^= static_cast<uint8_t>(*text++);
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t coordinate_hash(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t value = seed;
    value ^= x * 0x9E3779B9u;
    value ^= y * 0x85EBCA6Bu;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

static float signed_noise(uint32_t x, uint32_t y, uint32_t seed)
{
    const uint32_t value = coordinate_hash(x, y, seed);
    const float unit = float(value & 0x00FFFFFFu) / float(0x00FFFFFFu);
    return unit * 2.0f - 1.0f;
}

static void box_blur_3x3(
    const std::vector<float> &source,
    std::vector<float> &destination,
    unsigned width,
    unsigned height)
{
    destination.resize(source.size());

    for (unsigned y = 0; y < height; y++)
    {
        for (unsigned x = 0; x < width; x++)
        {
            float sum = 0.0f;
            float weightSum = 0.0f;

            for (int oy = -1; oy <= 1; oy++)
            {
                const int sy = clamp_index(int(y) + oy, int(height));
                const float wy = oy == 0 ? 2.0f : 1.0f;

                for (int ox = -1; ox <= 1; ox++)
                {
                    const int sx = clamp_index(int(x) + ox, int(width));
                    const float wx = ox == 0 ? 2.0f : 1.0f;
                    const float weight = wx * wy;
                    sum += source[unsigned(sy) * width + unsigned(sx)] * weight;
                    weightSum += weight;
                }
            }

            destination[y * width + x] = sum / weightSum;
        }
    }
}

static void blur_repeated(
    const std::vector<float> &source,
    std::vector<float> &destination,
    unsigned width,
    unsigned height,
    int passes)
{
    if (passes <= 0)
    {
        destination = source;
        return;
    }

    std::vector<float> a = source;
    std::vector<float> b;

    for (int pass = 0; pass < passes; pass++)
    {
        box_blur_3x3(a, b, width, height);
        a.swap(b);
    }

    destination.swap(a);
}

// Compare neighbours before adding noise. Big shapes should still read as big shapes.
static void sample_gradient(
    const std::vector<float> &field,
    unsigned width,
    unsigned height,
    unsigned x,
    unsigned y,
    float &outDx,
    float &outDy)
{
    const unsigned xLeft = unsigned(clamp_index(int(x) - 1, int(width)));
    const unsigned xRight = unsigned(clamp_index(int(x) + 1, int(width)));
    const unsigned yTop = unsigned(clamp_index(int(y) - 1, int(height)));
    const unsigned yBottom = unsigned(clamp_index(int(y) + 1, int(height)));

    outDx = field[y * width + xRight] - field[y * width + xLeft];
    outDy = field[yBottom * width + x] - field[yTop * width + x];
}

static float gradient_rms(
    const std::vector<float> &field,
    unsigned width,
    unsigned height)
{
    double sumSquares = 0.0;
    const size_t count = size_t(width) * size_t(height);

    for (unsigned y = 0; y < height; y++)
    {
        for (unsigned x = 0; x < width; x++)
        {
            float dx = 0.0f;
            float dy = 0.0f;
            sample_gradient(field, width, height, x, y, dx, dy);
            sumSquares += double(dx) * double(dx) + double(dy) * double(dy);
        }
    }

    if (count == 0)
        return 1.0f;

    const float rms = float(std::sqrt(sumSquares / double(count)));
    return rms > 1.0e-6f ? rms : 1.0f;
}

inline bool makeNormals(const std::vector<unsigned char>& rgba,unsigned width,unsigned height,const char* name,std::vector<unsigned char>& outRgba){
 if(!width||!height||width>4096||height>4096||rgba.size()!=size_t(width)*height*4)return false;
 const float amplitude=1.8f;const size_t pixelCount=size_t(width)*height;
 std::vector<float> rawHeight(pixelCount);
 float lo=1,hi=0;
 for(size_t i=0;i<pixelCount;i++){float h=(rgba[i*4]*.299f+rgba[i*4+1]*.587f+rgba[i*4+2]*.114f)/255.f;rawHeight[i]=h;lo=std::min(lo,h);hi=std::max(hi,h);}
 // Do not stretch a few palette quantization steps into full-height relief.
 const float heightRange=std::max(.25f,hi-lo);
 for(float& h:rawHeight)h=(h-lo)/heightRange;
    // Broad structural slope.  Blurring the height field before differentiating
    // gives bevels several pixels of width instead of a one-pixel cyan/magenta
    // edge.  Mixing a little raw height back in preserves lettering and trim.
    std::vector<float> blurredHeight;
    blur_repeated(rawHeight, blurredHeight, width, height, 1);

    std::vector<float> structuralHeight(pixelCount);
    for (size_t index = 0; index < pixelCount; index++)
    {
        structuralHeight[index] =
            blurredHeight[index] * kStructuralBlurMix +
            rawHeight[index] * (1.0f - kStructuralBlurMix);
    }

    // Albedo supplies structure, not random micro-grain. Procedural noise
    // made flat and dithered retail surfaces sparkle under moving light.
    (void)name;

    const float strength = amplitude > 0.0f
        ? amplitude
        : kDefaultNormalStrength;

    outRgba.resize(pixelCount * 4);

    for (unsigned y = 0; y < height; y++)
    {
        for (unsigned x = 0; x < width; x++)
        {
            float structuralDx = 0.0f;
            float structuralDy = 0.0f;
            float rawDx = 0.0f;
            float rawDy = 0.0f;

            sample_gradient(
                structuralHeight,
                width,
                height,
                x,
                y,
                structuralDx,
                structuralDy);

            sample_gradient(
                rawHeight,
                width,
                height,
                x,
                y,
                rawDx,
                rawDy);

            const float highFrequencyDx = rawDx - structuralDx;
            const float highFrequencyDy = rawDy - structuralDy;

            const float baseDx =
                structuralDx + highFrequencyDx * kFineHeightWeight;
            const float baseDy =
                structuralDy + highFrequencyDy * kFineHeightWeight;

            // Height-field normal is (-dh/du, -dh/dv, 1).
            float nx = -baseDx * strength;
            float ny = -baseDy * strength;
            float nz = 1.0f;

            const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (length > 1.0e-6f)
            {
                nx /= length;
                ny /= length;
                nz /= length;
            }
            else
            {
                nx = 0.0f;
                ny = 0.0f;
                nz = 1.0f;
            }

            const size_t index = (size_t(y) * width + x) * 4;
            outRgba[index + 0] = static_cast<unsigned char>(
                clamp01(nx * 0.5f + 0.5f) * 255.0f + 0.5f);
            outRgba[index + 1] = static_cast<unsigned char>(
                clamp01(ny * 0.5f + 0.5f) * 255.0f + 0.5f);
            outRgba[index + 2] = static_cast<unsigned char>(
                clamp01(nz * 0.5f + 0.5f) * 255.0f + 0.5f);
            outRgba[index + 3] = static_cast<unsigned char>(
                clamp01(rawHeight[size_t(y) * width + x]) * 255.0f + 0.5f);
        }
    }

    
    
    return true;
}

}
