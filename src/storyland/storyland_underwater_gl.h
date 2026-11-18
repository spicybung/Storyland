#pragma once

#include "src/graphics/storyland_underwater.h"
#include <string>

#ifndef APIENTRY
#define APIENTRY
#endif

inline constexpr const char* StorylandUnderwaterVertexGl=R"GLSL(#version 120
varying vec2 uv;
void main(){gl_Position=gl_Vertex;uv=gl_MultiTexCoord0.xy;}
)GLSL";

inline constexpr const char* StorylandUnderwaterFragmentGl=R"GLSL(#version 120
uniform sampler2D sceneTexture,depthTexture;
uniform mat4 inverseViewProjection;
uniform vec3 cameraPosition,fogColor,extinction;
uniform vec4 waterColorGain,parameters;
uniform int volumeCount;
uniform vec4 waterRectangle[16];
uniform vec2 waterHeight[16];
varying vec2 uv;
vec3 worldPosition(vec2 point,float depth) {
 vec4 world=inverseViewProjection*vec4(point*2.0-1.0,depth*2.0-1.0,1.0);
 return world.xyz/world.w;
}
float waterDistance(vec3 endpoint) {
 vec3 direction=endpoint-cameraPosition;
 vec3 reciprocal=1.0/(sign(direction)*max(abs(direction),vec3(.000001))+vec3(equal(direction,vec3(0.0)))*.000001);
 float covered=0.0;
 for(int volume=0;volume<16;++volume) {
  if(volume>=volumeCount)break;
  vec4 rectangle=waterRectangle[volume];
  vec3 lower=vec3(rectangle.x,rectangle.z,waterHeight[volume].y);
  vec3 upper=vec3(rectangle.y,rectangle.w,waterHeight[volume].x);
  vec3 first=(lower-cameraPosition)*reciprocal,last=(upper-cameraPosition)*reciprocal;
  vec3 nearPoint=min(first,last),farPoint=max(first,last);
  float enter=max(0.0,max(nearPoint.x,max(nearPoint.y,nearPoint.z)));
  float leave=min(1.0,min(farPoint.x,min(farPoint.y,farPoint.z)));
  covered+=max(0.0,leave-enter);
 }
 return min(covered,1.0)*length(direction);
}
void main() {
 float depth=texture2D(depthTexture,uv).r;
 float distance=waterDistance(worldPosition(uv,depth));
 float wet=1.0-exp(-distance*.16);
 // SA 0x7039C0: interpolate sine displacement at each 24-pixel reference strip.
 float rasterY=(1.0-uv.y)*448.0,strip=floor(rasterY/24.0)*24.0;
 float wave=mix(sin(strip*.04+parameters.x*1.5),sin((strip+24.0)*.04+parameters.x*1.5),fract(rasterY/24.0));
 vec2 shifted=clamp(uv+vec2(wave*(4.0/640.0)*parameters.y*wet,0.0),parameters.zw*.5,vec2(1.0)-parameters.zw*.5);
 vec4 source=texture2D(sceneTexture,shifted);
 float shiftedDistance=waterDistance(worldPosition(shifted,texture2D(depthTexture,shifted).r));
 if(abs(shiftedDistance-distance)>max(1.0,distance*.2))source=texture2D(sceneTexture,uv);
 vec3 transmission=exp(-extinction*distance);
 vec3 color=source.rgb*mix(vec3(1.0),waterColorGain.rgb,wet)*mix(1.0,waterColorGain.w,parameters.y);
 gl_FragColor=vec4(color*transmission+fogColor*(1.0-transmission),source.a);
}
)GLSL";

class StorylandUnderwaterGl {
public:
    std::string diagnostic;

    template<class GetProc>
    bool draw(const StorylandUnderwaterFrame& frame,const void* context,GetProc getProc) {
        if(!frame.enabled||!context)return false;
        if(context!=currentContext) {
            currentContext=context;program=0;textures={};width=height=0;
            if(!load(getProc))return false;
        }
        if(!program)return false;
        GLint viewport[4]{},oldProgram=0,oldActive=0,oldMatrix=0,oldTextures[2]{};
        glGetIntegerv(GL_VIEWPORT,viewport);
        if(viewport[2]<=0||viewport[3]<=0)return false;
        glGetIntegerv(0x8B8D,&oldProgram);glGetIntegerv(0x84E0,&oldActive);
        glGetIntegerv(GL_MATRIX_MODE,&oldMatrix);
        glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        GLint unpackBuffer=0;glGetIntegerv(0x88EF,&unpackBuffer);bindBuffer(0x88EC,0);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
        for(unsigned unit=0;unit<2;++unit) {
            activeTexture(0x84C0+unit);glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTextures[unit]);
        }
        if(!textures[0])glGenTextures(2,textures.data());
        if(width!=viewport[2]||height!=viewport[3]) {
            width=viewport[2];height=viewport[3];
            for(unsigned index=0;index<2;++index) {
                glBindTexture(GL_TEXTURE_2D,textures[index]);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,index?GL_NEAREST:GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,index?GL_NEAREST:GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
                if(index)glTexParameteri(GL_TEXTURE_2D,0x884C,GL_NONE);
                glTexImage2D(GL_TEXTURE_2D,0,index?0x81A6:0x8058,width,height,0,
                    index?GL_DEPTH_COMPONENT:GL_RGBA,index?GL_UNSIGNED_INT:GL_UNSIGNED_BYTE,nullptr);
            }
        }
        for(unsigned unit=0;unit<2;++unit) {
            activeTexture(0x84C0+unit);glBindTexture(GL_TEXTURE_2D,textures[unit]);
            glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,viewport[0],viewport[1],width,height);
        }
        bindBuffer(0x88EC,GLuint(unpackBuffer));glPopClientAttrib();
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_ALPHA_TEST);
        glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_FOG);
        glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_DITHER);glDisable(0x8DB9);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        useProgram(program);
        uniform1i(location("sceneTexture"),0);uniform1i(location("depthTexture"),1);
        uniform1i(location("volumeCount"),GLint(frame.volumeCount));
        uniform3fv(location("cameraPosition"),1,frame.camera.data());
        uniform3fv(location("fogColor"),1,frame.fogColor.data());
        uniform3fv(location("extinction"),1,frame.extinction.data());
        uniform4f(location("waterColorGain"),frame.colorGain[0],frame.colorGain[1],frame.colorGain[2],
            std::max(.04f,1.0f-frame.cameraDepth/90.0f));
        uniformMatrix4fv(location("inverseViewProjection"),1,GL_FALSE,frame.inverseViewProjection.data());
        uniform4f(location("parameters"),frame.gameSeconds,std::clamp((frame.immersion-.535f)/.465f,0.0f,1.0f),1.0f/width,1.0f/height);
        std::array<float,64> rectangles{};std::array<float,32> heights{};
        for(unsigned index=0;index<frame.volumeCount;++index) {
            const auto& volume=frame.volumes[index];
            rectangles[index*4]=volume.west;rectangles[index*4+1]=volume.east;
            rectangles[index*4+2]=volume.south;rectangles[index*4+3]=volume.north;
            heights[index*2]=volume.surface;heights[index*2+1]=volume.bottom;
        }
        uniform4fv(location("waterRectangle[0]"),GLsizei(frame.volumeCount),rectangles.data());
        uniform2fv(location("waterHeight[0]"),GLsizei(frame.volumeCount),heights.data());
        glBegin(GL_QUADS);
        glTexCoord2f(0,0);glVertex2f(-1,-1);glTexCoord2f(1,0);glVertex2f(1,-1);
        glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(-1,1);
        glEnd();
        useProgram(GLuint(oldProgram));glPopAttrib();
        for(unsigned unit=0;unit<2;++unit) {
            activeTexture(0x84C0+unit);glBindTexture(GL_TEXTURE_2D,GLuint(oldTextures[unit]));
        }
        activeTexture(GLenum(oldActive));glMatrixMode(GLenum(oldMatrix));
        return true;
    }

    void release(const void* context) {
        if(context&&context==currentContext) {
            if(program)deleteProgram(program);
            if(textures[0])glDeleteTextures(2,textures.data());
        }
        currentContext=nullptr;program=0;textures={};width=height=0;
    }

private:
    const void* currentContext=nullptr;
    GLuint program=0;
    std::array<GLuint,2> textures{};
    int width=0,height=0;
    GLuint (APIENTRY* createShader)(GLenum)=nullptr;
    void (APIENTRY* shaderSource)(GLuint,GLsizei,const char* const*,const GLint*)=nullptr;
    void (APIENTRY* compileShader)(GLuint)=nullptr;
    void (APIENTRY* getShaderiv)(GLuint,GLenum,GLint*)=nullptr;
    void (APIENTRY* getShaderInfoLog)(GLuint,GLsizei,GLsizei*,char*)=nullptr;
    GLuint (APIENTRY* createProgram)()=nullptr;
    void (APIENTRY* attachShader)(GLuint,GLuint)=nullptr;
    void (APIENTRY* linkProgram)(GLuint)=nullptr;
    void (APIENTRY* getProgramiv)(GLuint,GLenum,GLint*)=nullptr;
    void (APIENTRY* deleteShader)(GLuint)=nullptr;
    void (APIENTRY* deleteProgram)(GLuint)=nullptr;
    void (APIENTRY* useProgram)(GLuint)=nullptr;
    GLint (APIENTRY* getUniformLocation)(GLuint,const char*)=nullptr;
    void (APIENTRY* uniform1i)(GLint,GLint)=nullptr;
    void (APIENTRY* uniform2fv)(GLint,GLsizei,const GLfloat*)=nullptr;
    void (APIENTRY* uniform3fv)(GLint,GLsizei,const GLfloat*)=nullptr;
    void (APIENTRY* uniform4fv)(GLint,GLsizei,const GLfloat*)=nullptr;
    void (APIENTRY* uniform4f)(GLint,GLfloat,GLfloat,GLfloat,GLfloat)=nullptr;
    void (APIENTRY* uniformMatrix4fv)(GLint,GLsizei,GLboolean,const GLfloat*)=nullptr;
    void (APIENTRY* activeTexture)(GLenum)=nullptr;
    void (APIENTRY* bindBuffer)(GLenum,GLuint)=nullptr;

    GLint location(const char* name)const{return getUniformLocation(program,name);}

    template<class GetProc>
    bool load(GetProc getProc) {
#define STORYLAND_UNDERWATER_PROC(member,name) member=reinterpret_cast<decltype(member)>(getProc(name));if(!member){diagnostic="Underwater OpenGL entry point unavailable: " name;return false;}
        STORYLAND_UNDERWATER_PROC(createShader,"glCreateShader");
        STORYLAND_UNDERWATER_PROC(shaderSource,"glShaderSource");
        STORYLAND_UNDERWATER_PROC(compileShader,"glCompileShader");
        STORYLAND_UNDERWATER_PROC(getShaderiv,"glGetShaderiv");
        STORYLAND_UNDERWATER_PROC(getShaderInfoLog,"glGetShaderInfoLog");
        STORYLAND_UNDERWATER_PROC(createProgram,"glCreateProgram");
        STORYLAND_UNDERWATER_PROC(attachShader,"glAttachShader");
        STORYLAND_UNDERWATER_PROC(linkProgram,"glLinkProgram");
        STORYLAND_UNDERWATER_PROC(getProgramiv,"glGetProgramiv");
        STORYLAND_UNDERWATER_PROC(deleteShader,"glDeleteShader");
        STORYLAND_UNDERWATER_PROC(deleteProgram,"glDeleteProgram");
        STORYLAND_UNDERWATER_PROC(useProgram,"glUseProgram");
        STORYLAND_UNDERWATER_PROC(getUniformLocation,"glGetUniformLocation");
        STORYLAND_UNDERWATER_PROC(uniform1i,"glUniform1i");
        STORYLAND_UNDERWATER_PROC(uniform2fv,"glUniform2fv");
        STORYLAND_UNDERWATER_PROC(uniform3fv,"glUniform3fv");
        STORYLAND_UNDERWATER_PROC(uniform4fv,"glUniform4fv");
        STORYLAND_UNDERWATER_PROC(uniform4f,"glUniform4f");
        STORYLAND_UNDERWATER_PROC(uniformMatrix4fv,"glUniformMatrix4fv");
        STORYLAND_UNDERWATER_PROC(activeTexture,"glActiveTexture");
        STORYLAND_UNDERWATER_PROC(bindBuffer,"glBindBuffer");
#undef STORYLAND_UNDERWATER_PROC
        const GLuint vertex=createShader(0x8B31),fragment=createShader(0x8B30);
        shaderSource(vertex,1,&StorylandUnderwaterVertexGl,nullptr);
        shaderSource(fragment,1,&StorylandUnderwaterFragmentGl,nullptr);
        compileShader(vertex);compileShader(fragment);
        GLint vertexValid=0,fragmentValid=0;
        getShaderiv(vertex,0x8B81,&vertexValid);getShaderiv(fragment,0x8B81,&fragmentValid);
        if(vertexValid&&fragmentValid) {
            program=createProgram();attachShader(program,vertex);attachShader(program,fragment);linkProgram(program);
            GLint linked=0;getProgramiv(program,0x8B82,&linked);
            if(!linked){deleteProgram(program);program=0;diagnostic="Underwater shader link failed.";}
        } else {
            std::array<char,2048> log{};
            getShaderInfoLog(fragmentValid?vertex:fragment,GLsizei(log.size()),nullptr,log.data());
            diagnostic=std::string("Underwater shader compilation failed: ")+log.data();
        }
        deleteShader(vertex);deleteShader(fragment);
        return program!=0;
    }
};
