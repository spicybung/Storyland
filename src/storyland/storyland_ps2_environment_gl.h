#pragma once

#include "src/graphics/storyland_ps2_environment.h"
#include "src/graphics/storyland_ps2_environment_shader.h"
#include <array>
#include <cstring>
#include <string>

#ifndef APIENTRY
#define APIENTRY
#endif

class StorylandPs2EnvironmentGl {
public:
    template<class GetProc>
    bool capture(const void* context, GetProc getProc,
        const StorylandPs2EnvironmentImageView& corona, float heading,
        GLuint& output, std::string& error, const StorylandPs2EnvironmentState& state = {}) {
        if (!context || !storylandPs2EnvironmentImageValid(corona) || corona.bottomUp || corona.bgra ||
            corona.storageWidth || corona.storageHeight ||
            (corona.stride && corona.stride != size_t(corona.width) * 4) ||
            (corona.width & (corona.width-1)) || (corona.height & (corona.height-1))) {
            error = "The GPU environment requires a packed original RGBA coronastar raster.";
            return false;
        }
        if (currentContext != context) {
            currentContext = context;
            program = framebuffer = sourceTexture = coronaTexture = 0;
            width = height = coronaWidth = coronaHeight = 0;
            if (!load(getProc, error)) return false;
        }
        if (!program) { error = "The GPU environment shader is unavailable."; return false; }
        GLint viewport[4]{}, previousProgram=0, previousDraw=0, previousRead=0, previousActive=0;
        GLint previousTextures[2]{}, previousUnpackBuffer=0;
        glGetIntegerv(GL_VIEWPORT, viewport);
        if (viewport[2] <= 0 || viewport[3] <= 0 || viewport[2] > 16384 || viewport[3] > 16384) {
            error = "The GPU environment source viewport is invalid.";
            return false;
        }
        glGetIntegerv(0x8B8D, &previousProgram);
        glGetIntegerv(0x8CA6, &previousDraw);
        glGetIntegerv(0x8CAA, &previousRead);
        glGetIntegerv(0x84E0, &previousActive);
        for (unsigned unit=0;unit<2;++unit) {
            activeTexture(0x84C0+unit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextures[unit]);
        }
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        glGetIntegerv(0x88EF, &previousUnpackBuffer);
        bindBuffer(0x88EC, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        const auto restore = [&] {
            bindBuffer(0x88EC, GLuint(previousUnpackBuffer));
            glPopClientAttrib();
            bindFramebuffer(0x8D40, GLuint(previousDraw));
            bindFramebuffer(0x8CA8, GLuint(previousRead));
            useProgram(GLuint(previousProgram));
            glPopAttrib();
            for (unsigned unit=0;unit<2;++unit) {
                activeTexture(0x84C0+unit);
                glBindTexture(GL_TEXTURE_2D, GLuint(previousTextures[unit]));
            }
            activeTexture(GLenum(previousActive));
        };
        activeTexture(0x84C0);
        if (!sourceTexture) glGenTextures(1, &sourceTexture);
        if (width != viewport[2] || height != viewport[3]) {
            width = viewport[2]; height = viewport[3];
            allocate(sourceTexture, width, height, false);
        } else glBindTexture(GL_TEXTURE_2D, sourceTexture);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1], width, height);
        activeTexture(0x84C1);
        if (!coronaTexture) glGenTextures(1, &coronaTexture);
        if (coronaWidth != corona.width || coronaHeight != corona.height) {
            coronaWidth = corona.width; coronaHeight = corona.height;
            allocate(coronaTexture, coronaWidth, coronaHeight, false);
        } else glBindTexture(GL_TEXTURE_2D, coronaTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, coronaWidth, coronaHeight,
            GL_RGBA, GL_UNSIGNED_BYTE, corona.pixels);
        if (!output) {
            glGenTextures(1, &output);
            allocate(output, StorylandPs2EnvironmentSize, StorylandPs2EnvironmentSize, true);
            glBindTexture(GL_TEXTURE_2D, coronaTexture);
        }
        if (glGetError() != GL_NO_ERROR) {
            error = "GPU environment texture allocation or framebuffer copy failed.";
            restore(); return false;
        }
        if (!framebuffer) genFramebuffers(1, &framebuffer);
        bindFramebuffer(0x8D40, framebuffer);
        framebufferTexture2D(0x8D40, 0x8CE0, GL_TEXTURE_2D, output, 0);
        glDrawBuffer(0x8CE0);
        if (checkFramebufferStatus(0x8D40) != 0x8CD5) {
            error = "The GPU environment framebuffer is incomplete.";
            restore(); return false;
        }
        glViewport(0, 0, StorylandPs2EnvironmentSize, StorylandPs2EnvironmentSize);
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST); glDisable(GL_DITHER); glDisable(0x8DB9);
        for (unsigned plane=0;plane<6;++plane) glDisable(GL_CLIP_PLANE0+plane);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        useProgram(program);
        uniform1i(getUniformLocation(program, "framebufferTexture"), 0);
        uniform1i(getUniformLocation(program, "coronaTexture"), 1);
        uniform4f(getUniformLocation(program, "imageSizes"), float(width), float(height),
            float(coronaWidth), float(coronaHeight));
        const auto plan = storylandPs2EnvironmentPlan(heading);
        std::array<float,16> rectangles{}, dither{};
        for (size_t index=0;index<plan.corona.size();++index) {
            const auto& rect = plan.corona[index];
            rectangles[index*4] = float(rect.left);
            rectangles[index*4+1] = float(rect.top);
            rectangles[index*4+2] = float(rect.right);
            rectangles[index*4+3] = float(rect.bottom);
        }
        for (int y=0;y<4;++y) for (int x=0;x<4;++x)
            dither[size_t(y)*4+x] = float(storylandPs2EnvironmentDither(state, x, y));
        uniform4fv(getUniformLocation(program, "coronaRects"), 4, rectangles.data());
        uniform4fv(getUniformLocation(program, "ditherRows"), 4, dither.data());
        glBegin(GL_QUADS);
        glVertex2f(-1,-1); glVertex2f(1,-1); glVertex2f(1,1); glVertex2f(-1,1);
        glEnd();
        const bool success = glGetError() == GL_NO_ERROR;
        restore();
        if (success) error.clear();
        else error = "GPU environment composition failed.";
        return success;
    }

    void release(const void* context) {
        if (context && context == currentContext) {
            if (program && deleteProgram) deleteProgram(program);
            if (framebuffer && deleteFramebuffers) deleteFramebuffers(1, &framebuffer);
            if (sourceTexture) glDeleteTextures(1, &sourceTexture);
            if (coronaTexture) glDeleteTextures(1, &coronaTexture);
        }
        currentContext = nullptr;
        program = framebuffer = sourceTexture = coronaTexture = 0;
        width = height = coronaWidth = coronaHeight = 0;
    }

private:
    const void* currentContext = nullptr;
    GLuint program=0, framebuffer=0, sourceTexture=0, coronaTexture=0;
    int width=0, height=0, coronaWidth=0, coronaHeight=0;
    GLuint (APIENTRY* createShader)(GLenum)=nullptr;
    void (APIENTRY* shaderSource)(GLuint,GLsizei,const char* const*,const GLint*)=nullptr;
    void (APIENTRY* compileShader)(GLuint)=nullptr;
    void (APIENTRY* getShaderiv)(GLuint,GLenum,GLint*)=nullptr;
    void (APIENTRY* getShaderInfoLog)(GLuint,GLsizei,GLsizei*,char*)=nullptr;
    GLuint (APIENTRY* createProgram)()=nullptr;
    void (APIENTRY* attachShader)(GLuint,GLuint)=nullptr;
    void (APIENTRY* linkProgram)(GLuint)=nullptr;
    void (APIENTRY* getProgramiv)(GLuint,GLenum,GLint*)=nullptr;
    void (APIENTRY* getProgramInfoLog)(GLuint,GLsizei,GLsizei*,char*)=nullptr;
    void (APIENTRY* deleteShader)(GLuint)=nullptr;
    void (APIENTRY* deleteProgram)(GLuint)=nullptr;
    void (APIENTRY* useProgram)(GLuint)=nullptr;
    GLint (APIENTRY* getUniformLocation)(GLuint,const char*)=nullptr;
    void (APIENTRY* uniform1i)(GLint,GLint)=nullptr;
    void (APIENTRY* uniform4f)(GLint,GLfloat,GLfloat,GLfloat,GLfloat)=nullptr;
    void (APIENTRY* uniform4fv)(GLint,GLsizei,const GLfloat*)=nullptr;
    void (APIENTRY* activeTexture)(GLenum)=nullptr;
    void (APIENTRY* genFramebuffers)(GLsizei,GLuint*)=nullptr;
    void (APIENTRY* deleteFramebuffers)(GLsizei,const GLuint*)=nullptr;
    void (APIENTRY* bindFramebuffer)(GLenum,GLuint)=nullptr;
    void (APIENTRY* framebufferTexture2D)(GLenum,GLenum,GLenum,GLuint,GLint)=nullptr;
    GLenum (APIENTRY* checkFramebufferStatus)(GLenum)=nullptr;
    void (APIENTRY* bindBuffer)(GLenum,GLuint)=nullptr;

    template<class GetProc>
    bool load(GetProc getProc, std::string& error) {
#define STORYLAND_ENVIRONMENT_PROC(member,name) \
        member=reinterpret_cast<decltype(member)>(getProc(name)); \
        if(!member) {error=std::string("GPU environment function unavailable: ")+name;return false;}
        STORYLAND_ENVIRONMENT_PROC(createShader,"glCreateShader");
        STORYLAND_ENVIRONMENT_PROC(shaderSource,"glShaderSource");
        STORYLAND_ENVIRONMENT_PROC(compileShader,"glCompileShader");
        STORYLAND_ENVIRONMENT_PROC(getShaderiv,"glGetShaderiv");
        STORYLAND_ENVIRONMENT_PROC(getShaderInfoLog,"glGetShaderInfoLog");
        STORYLAND_ENVIRONMENT_PROC(createProgram,"glCreateProgram");
        STORYLAND_ENVIRONMENT_PROC(attachShader,"glAttachShader");
        STORYLAND_ENVIRONMENT_PROC(linkProgram,"glLinkProgram");
        STORYLAND_ENVIRONMENT_PROC(getProgramiv,"glGetProgramiv");
        STORYLAND_ENVIRONMENT_PROC(getProgramInfoLog,"glGetProgramInfoLog");
        STORYLAND_ENVIRONMENT_PROC(deleteShader,"glDeleteShader");
        STORYLAND_ENVIRONMENT_PROC(deleteProgram,"glDeleteProgram");
        STORYLAND_ENVIRONMENT_PROC(useProgram,"glUseProgram");
        STORYLAND_ENVIRONMENT_PROC(getUniformLocation,"glGetUniformLocation");
        STORYLAND_ENVIRONMENT_PROC(uniform1i,"glUniform1i");
        STORYLAND_ENVIRONMENT_PROC(uniform4f,"glUniform4f");
        STORYLAND_ENVIRONMENT_PROC(uniform4fv,"glUniform4fv");
        STORYLAND_ENVIRONMENT_PROC(activeTexture,"glActiveTexture");
        STORYLAND_ENVIRONMENT_PROC(genFramebuffers,"glGenFramebuffers");
        STORYLAND_ENVIRONMENT_PROC(deleteFramebuffers,"glDeleteFramebuffers");
        STORYLAND_ENVIRONMENT_PROC(bindFramebuffer,"glBindFramebuffer");
        STORYLAND_ENVIRONMENT_PROC(framebufferTexture2D,"glFramebufferTexture2D");
        STORYLAND_ENVIRONMENT_PROC(checkFramebufferStatus,"glCheckFramebufferStatus");
        STORYLAND_ENVIRONMENT_PROC(bindBuffer,"glBindBuffer");
#undef STORYLAND_ENVIRONMENT_PROC
        const GLuint vertex=createShader(0x8B31), fragment=createShader(0x8B30);
        shaderSource(vertex,1,&StorylandPs2EnvironmentVertexGl,nullptr);
        shaderSource(fragment,1,&StorylandPs2EnvironmentFragmentGl,nullptr);
        compileShader(vertex); compileShader(fragment);
        GLint vertexValid=0, fragmentValid=0;
        getShaderiv(vertex,0x8B81,&vertexValid); getShaderiv(fragment,0x8B81,&fragmentValid);
        std::array<char,4096> diagnostic{};
        if (!vertexValid || !fragmentValid) {
            getShaderInfoLog(vertexValid?fragment:vertex,GLsizei(diagnostic.size()),nullptr,diagnostic.data());
            error="GPU environment shader compilation failed: "+std::string(diagnostic.data());
        } else {
            program=createProgram(); attachShader(program,vertex); attachShader(program,fragment);
            linkProgram(program);
            GLint linked=0; getProgramiv(program,0x8B82,&linked);
            if (!linked) {
                getProgramInfoLog(program,GLsizei(diagnostic.size()),nullptr,diagnostic.data());
                error="GPU environment shader linking failed: "+std::string(diagnostic.data());
                deleteProgram(program); program=0;
            }
        }
        deleteShader(vertex); deleteShader(fragment);
        return program!=0;
    }

    void allocate(GLuint texture, int textureWidth, int textureHeight, bool output) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, output?GL_LINEAR:GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, output?GL_LINEAR:GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, output?GL_REPEAT:0x812F);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, output?GL_REPEAT:0x812F);
        glTexParameteri(GL_TEXTURE_2D, 0x813D, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, 0x8058, textureWidth, textureHeight, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
};
