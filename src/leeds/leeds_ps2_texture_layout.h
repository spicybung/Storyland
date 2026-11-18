#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

inline std::vector<uint8_t> storylandUnswizzlePs2TextureIndices(
    const std::vector<uint8_t>& input, int width, int height) {
    if (width <= 0 || height <= 0 || input.size() < size_t(width)*size_t(height)) return {};
    std::vector<uint8_t> output(size_t(width)*size_t(height),0u);
    unsigned logWidth=0;
    for (unsigned value=1;value<unsigned(width);value*=2) ++logWidth;
    if (logWidth<2 || logWidth>20) return {};
    const uint32_t mask=(uint32_t(1) << (logWidth+2)) - 1u;
    for (int rowBase=0;rowBase<height;rowBase+=4) {
        const int rows=std::min(4,height-rowBase);
        const size_t base=size_t(rowBase)*size_t(width);
        const size_t count=size_t(rows)*size_t(width);
        for (int row=0;row<rows;++row) {
            const int y=rowBase+row;
            for (int x=0;x<width;++x) {
                uint32_t nx=uint32_t(x&7)^(uint32_t(((y>>1)^(y>>2))<<2));
                nx=(nx&7u)|(uint32_t((x>>1)&~7));
                const uint32_t ny=uint32_t(y&1)|uint32_t((y>>1)&~1);
                const uint32_t n=uint32_t((y>>1)&1)|(uint32_t((x>>3)&1)<<1);
                const uint32_t source=(n|(nx<<2)|(ny<<(logWidth+1)))&mask;
                if (source<count)output[base+size_t(row)*size_t(width)+size_t(x)]=input[base+source];
            }
        }
    }
    return output;
}
