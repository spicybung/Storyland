#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

inline constexpr unsigned StorylandTextureMaxLevel=0x813D;

template<class MipLevels>
size_t storylandAuthoredMipCount(int width,int height,const MipLevels& mipLevels) {
    if(width<=0||height<=0)return 0;
    size_t count=0;
    for(const auto& level:mipLevels) {
        if(width==1&&height==1)break;
        width=std::max(1,width/2);
        height=std::max(1,height/2);
        if(level.width!=width||level.height!=height||
            level.rgba.size()!=size_t(width)*size_t(height)*4)break;
        ++count;
    }
    return count;
}

template<class MipLevels>
void storylandUploadRgbaTexture(int width,int height,const uint8_t* rgba,const MipLevels& mipLevels) {
    const size_t count=storylandAuthoredMipCount(width,height,mipLevels);
    glTexParameteri(GL_TEXTURE_2D,StorylandTextureMaxLevel,GLint(count));
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,count?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    for(size_t index=0;index<count;++index) {
        const auto& level=mipLevels[index];
        glTexImage2D(GL_TEXTURE_2D,GLint(index+1),GL_RGBA,level.width,level.height,
            0,GL_RGBA,GL_UNSIGNED_BYTE,level.rgba.data());
    }
}

template<class Image>
void storylandUploadRgbaTexture(const Image& image) {
    storylandUploadRgbaTexture(image.width,image.height,image.rgba.data(),image.mipLevels);
}
