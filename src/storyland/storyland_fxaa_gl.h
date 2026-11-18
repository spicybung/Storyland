#pragma once

#include "src/shaders/storyland_fxaa_gl.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef APIENTRY
#define APIENTRY
#endif

class StorylandFxaaGl {
public:
    const std::string& status() const {return diagnostic;}

    template<class GetProc>
    bool draw(const void* context,GetProc getProc) {
        if(!context)return false;
        if(context!=currentContext) {
            currentContext=context;program=texture=framebuffer=0;width=height=0;
            diagnostic.clear();
            if(!load(getProc))return false;
        }
        if(!program)return false;
        GLint viewport[4]{},oldProgram=0,oldDraw=0,oldRead=0,oldReadBuffer=0,oldActive=0;
        glGetIntegerv(GL_VIEWPORT,viewport);
        if(viewport[2]<=0||viewport[3]<=0)return false;
        glGetIntegerv(0x8B8D,&oldProgram);
        glGetIntegerv(0x8CA6,&oldDraw);glGetIntegerv(0x8CAA,&oldRead);
        glGetIntegerv(GL_READ_BUFFER,&oldReadBuffer);
        GLint drawCount=1;
        if(oldDraw)glGetIntegerv(0x8824,&drawCount);
        drawCount=std::clamp(drawCount,1,32);
        std::vector<GLenum> drawBuffersState(size_t(drawCount),GL_NONE);
        for(int index=0;index<drawCount;++index) {
            GLint buffer=0;glGetIntegerv(0x8825+index,&buffer);
            drawBuffersState[size_t(index)]=GLenum(buffer);
        }
        while(drawBuffersState.size()>1&&drawBuffersState.back()==GL_NONE)drawBuffersState.pop_back();
        if(drawBuffersState[0]==GL_NONE)return false;
        glGetIntegerv(0x84E0,&oldActive);
        const bool rasterizerDiscard=modernContext&&glIsEnabled(0x8C89);
        const bool sampleMask=sampleMaskSupported&&glIsEnabled(0x8E51);
        activeTexture(0x84C0);
        GLint oldTexture=0,oldSampler=0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTexture);
        if(bindSampler) {glGetIntegerv(0x8919,&oldSampler);bindSampler(0,0);}
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        GLint unpackBuffer=0;
        glGetIntegerv(0x88EF,&unpackBuffer);bindBuffer(0x88EC,0);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
        glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_STENCIL_TEST);
        glDisable(GL_DITHER);glDisable(GL_COLOR_LOGIC_OP);
        glDisable(0x809D);glDisable(0x809E);glDisable(0x80A0);
        if(modernContext)glDisable(0x8C89);
        if(sampleMaskSupported)glDisable(0x8E51);
        if(srgbFramebuffer)glDisable(0x8DB9);
        for(unsigned index=0;index<6;++index)glDisable(GL_CLIP_PLANE0+index);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        if(!texture)glGenTextures(1,&texture);
        glBindTexture(GL_TEXTURE_2D,texture);
        bool success=texture!=0;
        if(success&&(width!=viewport[2]||height!=viewport[3])) {
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
            glTexParameteri(GL_TEXTURE_2D,0x813D,0);
            glTexImage2D(GL_TEXTURE_2D,0,0x8058,viewport[2],viewport[3],0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            GLint allocatedWidth=0,allocatedHeight=0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&allocatedWidth);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&allocatedHeight);
            success=allocatedWidth==viewport[2]&&allocatedHeight==viewport[3];
            if(success) {width=viewport[2];height=viewport[3];}
        }
        bindBuffer(0x88EC,GLuint(unpackBuffer));glPopClientAttrib();
        if(!framebuffer)genFramebuffers(1,&framebuffer);
        bindFramebuffer(0x8CA9,framebuffer);
        framebufferTexture2D(0x8CA9,0x8CE0,GL_TEXTURE_2D,texture,0);
        glDrawBuffer(0x8CE0);
        success=success&&checkFramebufferStatus(0x8CA9)==0x8CD5;
        bindFramebuffer(0x8CA8,GLuint(oldDraw));
        GLint sourceReadBuffer=0;glGetIntegerv(GL_READ_BUFFER,&sourceReadBuffer);
        const GLenum source=drawBuffersState[0]==GL_FRONT_AND_BACK?GL_BACK:drawBuffersState[0];
        glReadBuffer(source);
        if(success)blitFramebuffer(viewport[0],viewport[1],viewport[0]+width,viewport[1]+height,
            0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        glReadBuffer(GLenum(sourceReadBuffer));
        bindFramebuffer(0x8CA9,GLuint(oldDraw));
        glDrawBuffer(drawBuffersState[0]);
        if(success) {
            useProgram(program);
            uniform1i(getUniformLocation(program,"sceneTexture"),0);
            uniform2f(getUniformLocation(program,"inverseFrame"),1.0f/width,1.0f/height);
            glBindTexture(GL_TEXTURE_2D,texture);
            glViewport(viewport[0],viewport[1],width,height);
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(0,0);glVertex2f(-1,-1);
            glTexCoord2f(1,0);glVertex2f(1,-1);
            glTexCoord2f(0,1);glVertex2f(-1,1);
            glTexCoord2f(1,1);glVertex2f(1,1);
            glEnd();
        } else diagnostic="FXAA is unavailable for this view.";
        bindFramebuffer(0x8CA9,GLuint(oldDraw));
        if(oldDraw)drawBuffers(GLsizei(drawBuffersState.size()),drawBuffersState.data());
        else glDrawBuffer(drawBuffersState[0]);
        bindFramebuffer(0x8CA8,GLuint(oldRead));glReadBuffer(GLenum(oldReadBuffer));
        useProgram(GLuint(oldProgram));glPopAttrib();
        if(rasterizerDiscard)glEnable(0x8C89);
        if(sampleMask)glEnable(0x8E51);
        glBindTexture(GL_TEXTURE_2D,GLuint(oldTexture));
        if(bindSampler)bindSampler(0,GLuint(oldSampler));
        activeTexture(GLenum(oldActive));
        if(success)diagnostic.clear();
        return success;
    }

    void release(const void* context) {
        if(context&&context==currentContext) {
            if(program)deleteProgram(program);
            if(texture)glDeleteTextures(1,&texture);
            if(framebuffer)deleteFramebuffers(1,&framebuffer);
        }
        currentContext=nullptr;program=texture=framebuffer=0;width=height=0;diagnostic.clear();
    }

private:
    const void* currentContext=nullptr;
    GLuint program=0,texture=0,framebuffer=0;
    int width=0,height=0;
    bool modernContext=false,srgbFramebuffer=false,sampleMaskSupported=false;
    std::string diagnostic;
    GLuint (APIENTRY* createShader)(GLenum)=nullptr;
    void (APIENTRY* shaderSource)(GLuint,GLsizei,const char* const*,const GLint*)=nullptr;
    void (APIENTRY* compileShader)(GLuint)=nullptr;
    void (APIENTRY* getShaderiv)(GLuint,GLenum,GLint*)=nullptr;
    GLuint (APIENTRY* createProgram)()=nullptr;
    void (APIENTRY* attachShader)(GLuint,GLuint)=nullptr;
    void (APIENTRY* linkProgram)(GLuint)=nullptr;
    void (APIENTRY* getProgramiv)(GLuint,GLenum,GLint*)=nullptr;
    void (APIENTRY* deleteShader)(GLuint)=nullptr;
    void (APIENTRY* deleteProgram)(GLuint)=nullptr;
    void (APIENTRY* useProgram)(GLuint)=nullptr;
    GLint (APIENTRY* getUniformLocation)(GLuint,const char*)=nullptr;
    void (APIENTRY* uniform1i)(GLint,GLint)=nullptr;
    void (APIENTRY* uniform2f)(GLint,GLfloat,GLfloat)=nullptr;
    void (APIENTRY* activeTexture)(GLenum)=nullptr;
    void (APIENTRY* genFramebuffers)(GLsizei,GLuint*)=nullptr;
    void (APIENTRY* deleteFramebuffers)(GLsizei,const GLuint*)=nullptr;
    void (APIENTRY* bindFramebuffer)(GLenum,GLuint)=nullptr;
    void (APIENTRY* framebufferTexture2D)(GLenum,GLenum,GLenum,GLuint,GLint)=nullptr;
    GLenum (APIENTRY* checkFramebufferStatus)(GLenum)=nullptr;
    void (APIENTRY* blitFramebuffer)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum)=nullptr;
    void (APIENTRY* drawBuffers)(GLsizei,const GLenum*)=nullptr;
    void (APIENTRY* bindBuffer)(GLenum,GLuint)=nullptr;
    void (APIENTRY* bindSampler)(GLuint,GLuint)=nullptr;

    template<class GetProc>
    bool load(GetProc getProc) {
#define STORYLAND_FXAA_PROC(member,name) member=reinterpret_cast<decltype(member)>(getProc(name));if(!member){diagnostic="FXAA is unavailable on this device.";return false;}
        STORYLAND_FXAA_PROC(createShader,"glCreateShader");
        STORYLAND_FXAA_PROC(shaderSource,"glShaderSource");
        STORYLAND_FXAA_PROC(compileShader,"glCompileShader");
        STORYLAND_FXAA_PROC(getShaderiv,"glGetShaderiv");
        STORYLAND_FXAA_PROC(createProgram,"glCreateProgram");
        STORYLAND_FXAA_PROC(attachShader,"glAttachShader");
        STORYLAND_FXAA_PROC(linkProgram,"glLinkProgram");
        STORYLAND_FXAA_PROC(getProgramiv,"glGetProgramiv");
        STORYLAND_FXAA_PROC(deleteShader,"glDeleteShader");
        STORYLAND_FXAA_PROC(deleteProgram,"glDeleteProgram");
        STORYLAND_FXAA_PROC(useProgram,"glUseProgram");
        STORYLAND_FXAA_PROC(getUniformLocation,"glGetUniformLocation");
        STORYLAND_FXAA_PROC(uniform1i,"glUniform1i");
        STORYLAND_FXAA_PROC(uniform2f,"glUniform2f");
        STORYLAND_FXAA_PROC(activeTexture,"glActiveTexture");
        STORYLAND_FXAA_PROC(genFramebuffers,"glGenFramebuffers");
        STORYLAND_FXAA_PROC(deleteFramebuffers,"glDeleteFramebuffers");
        STORYLAND_FXAA_PROC(bindFramebuffer,"glBindFramebuffer");
        STORYLAND_FXAA_PROC(framebufferTexture2D,"glFramebufferTexture2D");
        STORYLAND_FXAA_PROC(checkFramebufferStatus,"glCheckFramebufferStatus");
        STORYLAND_FXAA_PROC(blitFramebuffer,"glBlitFramebuffer");
        STORYLAND_FXAA_PROC(drawBuffers,"glDrawBuffers");
        STORYLAND_FXAA_PROC(bindBuffer,"glBindBuffer");
#undef STORYLAND_FXAA_PROC
        int major=0,minor=0;
        const auto* version=reinterpret_cast<const char*>(glGetString(GL_VERSION));
        if(version)std::sscanf(version,"%d.%d",&major,&minor);
        modernContext=major>=3;
        sampleMaskSupported=major>3||(major==3&&minor>=2);
        const auto* extensions=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        srgbFramebuffer=modernContext||(extensions&&std::strstr(extensions,"framebuffer_sRGB"));
        const bool samplers=major>3||(major==3&&minor>=3)||
            (extensions&&std::strstr(extensions,"GL_ARB_sampler_objects"));
        bindSampler=samplers?reinterpret_cast<decltype(bindSampler)>(getProc("glBindSampler")):nullptr;
        const GLuint vertex=createShader(0x8B31),fragment=createShader(0x8B30);
        shaderSource(vertex,1,&StorylandFxaaVertexGl,nullptr);
        shaderSource(fragment,1,&StorylandFxaaFragmentGl,nullptr);
        compileShader(vertex);compileShader(fragment);
        GLint vertexValid=0,fragmentValid=0;
        getShaderiv(vertex,0x8B81,&vertexValid);getShaderiv(fragment,0x8B81,&fragmentValid);
        if(vertexValid&&fragmentValid) {
            program=createProgram();attachShader(program,vertex);attachShader(program,fragment);linkProgram(program);
            GLint linked=0;getProgramiv(program,0x8B82,&linked);
            if(!linked) {deleteProgram(program);program=0;}
        }
        deleteShader(vertex);deleteShader(fragment);
        if(!program)diagnostic="FXAA is unavailable on this device.";
        return program!=0;
    }
};
