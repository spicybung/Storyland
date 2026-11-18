#pragma once

#include "src/graphics/storyland_vehicle_environment.h"

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER 0x8D41
#define GL_RENDERBUFFER_BINDING 0x8CA7
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

struct StorylandVehicleEnvironmentGlApi {
    void (APIENTRY* genFramebuffers)(GLsizei,GLuint*) = nullptr;
    void (APIENTRY* bindFramebuffer)(GLenum,GLuint) = nullptr;
    void (APIENTRY* framebufferTexture2D)(GLenum,GLenum,GLenum,GLuint,GLint) = nullptr;
    GLenum (APIENTRY* checkFramebufferStatus)(GLenum) = nullptr;
    void (APIENTRY* deleteFramebuffers)(GLsizei,const GLuint*) = nullptr;
    void (APIENTRY* genRenderbuffers)(GLsizei,GLuint*) = nullptr;
    void (APIENTRY* bindRenderbuffer)(GLenum,GLuint) = nullptr;
    void (APIENTRY* renderbufferStorage)(GLenum,GLenum,GLsizei,GLsizei) = nullptr;
    void (APIENTRY* framebufferRenderbuffer)(GLenum,GLenum,GLenum,GLuint) = nullptr;
    void (APIENTRY* deleteRenderbuffers)(GLsizei,const GLuint*) = nullptr;

    bool valid() const {
        return genFramebuffers && bindFramebuffer && framebufferTexture2D &&
            checkFramebufferStatus && deleteFramebuffers && genRenderbuffers &&
            bindRenderbuffer && renderbufferStorage && framebufferRenderbuffer && deleteRenderbuffers;
    }
};

struct StorylandVehicleEnvironmentGlTarget {
    StorylandVehicleEnvironmentGlApi api;
    GLuint texture=0, framebuffer=0, depth=0;
    GLint previousFramebuffer=0;
    int faceSize=0;
    bool active=false;

    void release() {
        if (active) end();
        if (texture) glDeleteTextures(1,&texture);
        if (framebuffer && api.deleteFramebuffers) api.deleteFramebuffers(1,&framebuffer);
        if (depth && api.deleteRenderbuffers) api.deleteRenderbuffers(1,&depth);
        texture=framebuffer=depth=0;
        faceSize=0;
    }

    bool begin(int requestedSize, std::string& error) {
        if (!api.valid()) {error="Framebuffer capture is unavailable.";return false;}
        if (active) {error="Environment capture is already active.";return false;}
        requestedSize=std::clamp(requestedSize,32,256);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previousFramebuffer);
        GLint previousTexture=0, previousRenderbuffer=0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&previousTexture);
        glGetIntegerv(GL_RENDERBUFFER_BINDING,&previousRenderbuffer);
        if (faceSize!=requestedSize) {
            if(GLuint(previousTexture)==texture)previousTexture=0;
            if(GLuint(previousRenderbuffer)==depth)previousRenderbuffer=0;
            release();
            faceSize=requestedSize;
            glGenTextures(1,&texture);
            glBindTexture(GL_TEXTURE_2D,texture);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,faceSize*3,faceSize*2,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            api.genFramebuffers(1,&framebuffer);
            api.bindFramebuffer(GL_FRAMEBUFFER,framebuffer);
            api.framebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
            api.genRenderbuffers(1,&depth);
            api.bindRenderbuffer(GL_RENDERBUFFER,depth);
            api.renderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,faceSize*3,faceSize*2);
            api.framebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        } else api.bindFramebuffer(GL_FRAMEBUFFER,framebuffer);
        glBindTexture(GL_TEXTURE_2D,GLuint(previousTexture));
        api.bindRenderbuffer(GL_RENDERBUFFER,GLuint(previousRenderbuffer));
        if (!texture || !framebuffer || !depth ||
            api.checkFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {
            api.bindFramebuffer(GL_FRAMEBUFFER,GLuint(previousFramebuffer));
            release();
            error="The vehicle environment framebuffer is incomplete.";
            return false;
        }
        active=true;
        error.clear();
        return true;
    }

    void end() {
        if (active) api.bindFramebuffer(GL_FRAMEBUFFER,GLuint(previousFramebuffer));
        active=false;
    }
};
