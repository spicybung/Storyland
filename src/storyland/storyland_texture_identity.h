#pragma once
#include <cstddef>
#include <cstdint>
#include <tuple>

using StorylandTextureStorageKey = std::tuple<bool,uint64_t,uint64_t>;

inline StorylandTextureStorageKey storylandTextureStorageKey(bool inImg,
    size_t allocationOffset,size_t headerOffset) {
    return {inImg,uint64_t(allocationOffset),uint64_t(headerOffset)};
}
