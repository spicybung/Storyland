#pragma once

#include "src/graphics/storyland_graphics_quality.h"
#include "src/graphics/storyland_texture_upload.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

class StorylandTextureQualityGl {
public:
    float maximumAnisotropy(const void* context) {
        if(!context)return 1.0f;
        if(context!=currentContext) {
            currentContext=context;
            maximum=1.0f;
            supported=false;
            const auto* extensions=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
            const auto* version=reinterpret_cast<const char*>(glGetString(GL_VERSION));
            int major=0,minor=0;
            if(version)std::sscanf(version,"%d.%d",&major,&minor);
            supported=hasExtension(extensions,"GL_EXT_texture_filter_anisotropic")||
                hasExtension(extensions,"GL_ARB_texture_filter_anisotropic")||
                major>4||(major==4&&minor>=6);
            if(supported) {
                GLfloat limit=1;
                glGetFloatv(0x84FF,&limit);
                maximum=std::isfinite(limit)?std::max(1.0f,float(limit)):1.0f;
            }
        }
        return maximum;
    }

    void reset() {currentContext=nullptr;maximum=1.0f;supported=false;}

    void applyBoundMaterial(const StorylandGraphicsQualitySettings& settings,
        bool targetProfile,const void* context) {
        maximumAnisotropy(context);
        if(!context)return;
        GLint texture=0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        if(!texture)return;
        if(targetProfile) {
            GLint maximumLevel=0;
            glGetTexParameteriv(GL_TEXTURE_2D,StorylandTextureMaxLevel,&maximumLevel);
            if(maximumLevel>31) {
                GLint width=0,height=0;
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
                if(width<=0||height<=0)return;
                maximumLevel=0;
                while(width>1||height>1) {
                    width=std::max(1,width/2);height=std::max(1,height/2);
                    GLint nextWidth=0,nextHeight=0;
                    glGetTexLevelParameteriv(GL_TEXTURE_2D,maximumLevel+1,GL_TEXTURE_WIDTH,&nextWidth);
                    glGetTexLevelParameteriv(GL_TEXTURE_2D,maximumLevel+1,GL_TEXTURE_HEIGHT,&nextHeight);
                    if(nextWidth!=width||nextHeight!=height)break;
                    ++maximumLevel;
                }
                glTexParameteri(GL_TEXTURE_2D,StorylandTextureMaxLevel,maximumLevel);
            }
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
                maximumLevel>0?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
        }
        if(supported)glTexParameterf(GL_TEXTURE_2D,0x84FE,
            storylandRequestedAnisotropy(settings,targetProfile,maximum));
    }

private:
    const void* currentContext=nullptr;
    float maximum=1.0f;
    bool supported=false;

    static bool hasExtension(const char* extensions,const char* name) {
        if(!extensions)return false;
        const size_t length=std::strlen(name);
        for(const char* match=std::strstr(extensions,name);match;match=std::strstr(match+length,name))
            if((match==extensions||match[-1]==' ')&&(match[length]=='\0'||match[length]==' '))return true;
        return false;
    }
};
