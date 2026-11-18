#pragma once

#include "src/graphics/storyland_ps2_postfx.h"
#include <array>

#ifndef APIENTRY
#define APIENTRY
#endif

class StorylandPs2PostEffectsGl {
public:
    template<class GetProc>
    bool draw(const StorylandPs2PostEffects& settings, const void* context, GetProc getProc) {
        if (!settings.enabled() || !context) return false;
        if (context != currentContext) {
            currentContext=context;program=framebuffer=0;textures={};width=height=0;
            if (!load(getProc)) return false;
        }
        if (!program) return false;
        GLint viewport[4]{},oldProgram=0,oldFramebuffer=0,oldReadFramebuffer=0,oldActive=0,oldMatrix=0;
        glGetIntegerv(GL_VIEWPORT,viewport);
        if (viewport[2]<=0 || viewport[3]<=0) return false;
        glGetIntegerv(0x8B8D,&oldProgram);
        glGetIntegerv(0x8CA6,&oldFramebuffer);
        glGetIntegerv(0x8CAA,&oldReadFramebuffer);
        glGetIntegerv(0x84E0,&oldActive);
        glGetIntegerv(GL_MATRIX_MODE,&oldMatrix);
        GLint oldTexture[2]{};
        for (unsigned unit=0;unit<2;++unit) {
            activeTexture(0x84C0+unit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTexture[unit]);
        }
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        GLint unpackBuffer=0;glGetIntegerv(0x88EF,&unpackBuffer);
        bindBuffer(0x88EC,0);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        activeTexture(0x84C0);
        if (!textures[0]) glGenTextures(4,textures.data());
        if (width!=viewport[2] || height!=viewport[3]) {
            width=viewport[2];height=viewport[3];
            allocate(textures[0],width,height);allocate(textures[1],256,256);
            allocate(textures[2],256,256);allocate(textures[3],512,512);
        }
        bindFramebuffer(0x8CA8,GLuint(oldFramebuffer));
        glBindTexture(GL_TEXTURE_2D,textures[0]);
        glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,viewport[0],viewport[1],width,height);
        bindBuffer(0x88EC,GLuint(unpackBuffer));glPopClientAttrib();
        if (!framebuffer) genFramebuffers(1,&framebuffer);
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
        glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);glDisable(GL_DITHER);glDisable(0x8DB9);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        useProgram(program);
        uniform1i(getUniformLocation(program,"sourceTexture"),0);
        uniform1i(getUniformLocation(program,"originalTexture"),1);
        activeTexture(0x84C1);glBindTexture(GL_TEXTURE_2D,textures[0]);activeTexture(0x84C0);
        bool success=true;
        if (settings.radiosityIntensity) {
            success=target(textures[1],256,256);
            if (success) render(textures[0],0,float(settings.radiosityThreshold),0,0,0);
            for (unsigned pass=0;success && pass<4;++pass) {
                const GLuint source=textures[1+pass%2],destination=textures[2-pass%2];
                success=target(destination,256,256);
                if (success) render(source,pass%2?2:1,0,0,0,0);
            }
            bindFramebuffer(0x8D40,GLuint(oldFramebuffer));glViewport(viewport[0],viewport[1],width,height);
            if (success) {
                render(textures[1],3,float(settings.radiosityIntensity),0,0,0);
                glBindTexture(GL_TEXTURE_2D,textures[0]);
                glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,viewport[0],viewport[1],width,height);
            }
        }
        if (success && (settings.glowMix || settings.filter[0] || settings.filter[1] || settings.filter[2])) {
            success=target(textures[3],512,512);
            if (success) render(textures[0],6,0,0,0,0);
            bindFramebuffer(0x8D40,GLuint(oldFramebuffer));glViewport(viewport[0],viewport[1],width,height);
            if (success) {
                uniform4f(getUniformLocation(program,"filterColor"),float(settings.filter[0]),
                    float(settings.filter[1]),float(settings.filter[2]),0);
                render(textures[3],4,settings.glowOffset/float(width),settings.glowOffset/float(height),float(settings.glowMix),0);
            }
        }
        bindFramebuffer(0x8D40,GLuint(oldFramebuffer));bindFramebuffer(0x8CA8,GLuint(oldReadFramebuffer));
        useProgram(GLuint(oldProgram));glPopAttrib();
        for (unsigned unit=0;unit<2;++unit) {
            activeTexture(0x84C0+unit);glBindTexture(GL_TEXTURE_2D,GLuint(oldTexture[unit]));
        }
        activeTexture(GLenum(oldActive));glMatrixMode(GLenum(oldMatrix));
        return success;
    }

    void release(const void* context) {
        if (context && context==currentContext) {
            if (program) deleteProgram(program);
            if (framebuffer) deleteFramebuffers(1,&framebuffer);
            if (textures[0]) glDeleteTextures(4,textures.data());
        }
        currentContext=nullptr;program=framebuffer=0;textures={};width=height=0;
    }

private:
    const void* currentContext=nullptr;
    GLuint program=0,framebuffer=0;
    std::array<GLuint,4> textures{};
    int width=0,height=0;
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
    void (APIENTRY* uniform4f)(GLint,GLfloat,GLfloat,GLfloat,GLfloat)=nullptr;
    void (APIENTRY* activeTexture)(GLenum)=nullptr;
    void (APIENTRY* genFramebuffers)(GLsizei,GLuint*)=nullptr;
    void (APIENTRY* deleteFramebuffers)(GLsizei,const GLuint*)=nullptr;
    void (APIENTRY* bindFramebuffer)(GLenum,GLuint)=nullptr;
    void (APIENTRY* framebufferTexture2D)(GLenum,GLenum,GLenum,GLuint,GLint)=nullptr;
    GLenum (APIENTRY* checkFramebufferStatus)(GLenum)=nullptr;
    void (APIENTRY* bindBuffer)(GLenum,GLuint)=nullptr;

    template<class GetProc>
    bool load(GetProc getProc) {
#define STORYLAND_POSTFX_PROC(member,name) member=reinterpret_cast<decltype(member)>(getProc(name));if(!member)return false
        STORYLAND_POSTFX_PROC(createShader,"glCreateShader");
        STORYLAND_POSTFX_PROC(shaderSource,"glShaderSource");
        STORYLAND_POSTFX_PROC(compileShader,"glCompileShader");
        STORYLAND_POSTFX_PROC(getShaderiv,"glGetShaderiv");
        STORYLAND_POSTFX_PROC(createProgram,"glCreateProgram");
        STORYLAND_POSTFX_PROC(attachShader,"glAttachShader");
        STORYLAND_POSTFX_PROC(linkProgram,"glLinkProgram");
        STORYLAND_POSTFX_PROC(getProgramiv,"glGetProgramiv");
        STORYLAND_POSTFX_PROC(deleteShader,"glDeleteShader");
        STORYLAND_POSTFX_PROC(deleteProgram,"glDeleteProgram");
        STORYLAND_POSTFX_PROC(useProgram,"glUseProgram");
        STORYLAND_POSTFX_PROC(getUniformLocation,"glGetUniformLocation");
        STORYLAND_POSTFX_PROC(uniform1i,"glUniform1i");
        STORYLAND_POSTFX_PROC(uniform4f,"glUniform4f");
        STORYLAND_POSTFX_PROC(activeTexture,"glActiveTexture");
        STORYLAND_POSTFX_PROC(genFramebuffers,"glGenFramebuffers");
        STORYLAND_POSTFX_PROC(deleteFramebuffers,"glDeleteFramebuffers");
        STORYLAND_POSTFX_PROC(bindFramebuffer,"glBindFramebuffer");
        STORYLAND_POSTFX_PROC(framebufferTexture2D,"glFramebufferTexture2D");
        STORYLAND_POSTFX_PROC(checkFramebufferStatus,"glCheckFramebufferStatus");
        STORYLAND_POSTFX_PROC(bindBuffer,"glBindBuffer");
#undef STORYLAND_POSTFX_PROC
        const GLuint vertex=createShader(0x8B31),fragment=createShader(0x8B30);
        shaderSource(vertex,1,&StorylandPs2PostEffectsVertexGl,nullptr);
        shaderSource(fragment,1,&StorylandPs2PostEffectsFragmentGl,nullptr);
        compileShader(vertex);compileShader(fragment);
        GLint vertexValid=0,fragmentValid=0;
        getShaderiv(vertex,0x8B81,&vertexValid);getShaderiv(fragment,0x8B81,&fragmentValid);
        if (vertexValid && fragmentValid) {
            program=createProgram();attachShader(program,vertex);attachShader(program,fragment);linkProgram(program);
            GLint linked=0;getProgramiv(program,0x8B82,&linked);
            if (!linked) {deleteProgram(program);program=0;}
        }
        deleteShader(vertex);deleteShader(fragment);return program!=0;
    }

    void allocate(GLuint texture,int textureWidth,int textureHeight) {
        glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glTexImage2D(GL_TEXTURE_2D,0,0x8058,textureWidth,textureHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    }

    bool target(GLuint texture,int targetWidth,int targetHeight) {
        bindFramebuffer(0x8D40,framebuffer);
        framebufferTexture2D(0x8D40,0x8CE0,GL_TEXTURE_2D,texture,0);
        if (checkFramebufferStatus(0x8D40)!=0x8CD5) return false;
        glViewport(0,0,targetWidth,targetHeight);return true;
    }

    void render(GLuint source,int operation,float x,float y,float z,float w) {
        glBindTexture(GL_TEXTURE_2D,source);
        uniform1i(getUniformLocation(program,"operation"),operation);
        uniform4f(getUniformLocation(program,"parameters"),x,y,z,w);
        glBegin(GL_QUADS);
        glTexCoord2f(0,0);glVertex2f(-1,-1);glTexCoord2f(1,0);glVertex2f(1,-1);
        glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(-1,1);
        glEnd();
    }
};
