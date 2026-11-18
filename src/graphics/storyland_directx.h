#pragma once
// Dispatch the existing immediate-mode frontend to OpenGL or native Direct3D 9.
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <string>
#include <array>
#include <cstdint>
#include <vector>
#include "src/shaders/storyland_shaders.h"
#include "src/graphics/storyland_ps2_lighting.h"
#include "src/graphics/storyland_ps2_postfx.h"
#include "src/graphics/storyland_graphics_quality.h"
struct StorylandScreenDropletCamera;
struct StorylandUnderwaterFrame;
namespace storylandDx {
struct WaterSettings {
    float time=0,wind=0,rain=0,sunVisibility=1,fogStart=0,fogEnd=1;
    std::array<float,3> horizon{},zenith{},lower{},sunDirection{},sunColor{},fogColor{},camera{};
    bool textured=false;
    const uint8_t* reactionPixels=nullptr;
    int reactionWidth=0,reactionHeight=0;
    float reactionOriginX=0,reactionOriginY=0,reactionExtent=1;
};
#ifdef STORYLAND_ENABLE_DIRECTX
void setGraphicsQuality(const StorylandGraphicsQualitySettings& settings,bool targetProfile);
float maximumAnisotropy();
void drawFxaa();
bool active();
bool requested();
void selectFromCommandLine();
const std::string& error();
bool beginSurface(bool vcs, StorylandShaderKind kind, bool texture, int mode, int prelight,
    const float* ambient, const float* directional, const float* fog, const float* sunDirection, float start, float end,
    int consoleProfile=-1,const float* cameraView=nullptr,const float* consoleAmbient=nullptr,
    const float* consoleEmissive=nullptr,float textureScale=1.0f,float environmentStrength=1.0f);
void endSurface();
bool beginWater(const WaterSettings& settings);
void endWater();
void setMaterialEffect(float strength);
void setPrelightScale(float scale);
void setPspStripState(int lodMode,float lodSlope,float depthOffset);
void setPspPrelitModel(bool prelit);
void setPspLocalLight(unsigned index,const float* eyeDirection,const float* color);
void setPs2Lighting(const StorylandPs2LightingColors& colors,const StorylandPs2FogParameters& fog);
void setPs2PrelitModel(bool prelit);
void setPs2LocalLight(unsigned index,const float* eyeDirection,const float* color);
void setPs2EnvironmentCoefficient(float coefficient);
void setPs2BudokaiReflection(bool enabled);
void setPs2EnvironmentBasis(const float* s,const float* t);
void setSecondaryUv(float u,float v);
void setDualTexture(GLuint texture,int mode,float strength,const float* cameraView=nullptr,
    int faceSize=64,float scaleU=1.0f,float scaleV=1.0f,int uvSet=0);
GLuint beginVehicleEnvironmentCapture(int faceSize,std::string& error);
void endVehicleEnvironmentCapture();
void releaseVehicleEnvironmentCapture();
GLuint vehicleEnvironmentTexture();
BOOL present(HDC dc);
void drawFrameBlur(unsigned opacity);
void drawPs2PostEffects(const StorylandPs2PostEffects& settings);
void setUnderwater(const StorylandUnderwaterFrame& settings);
void drawUnderwater(const StorylandUnderwaterFrame& settings);
void drawScreenDroplets(bool enabled,float gameSeconds,std::uint32_t waterSerial,
    std::uint32_t bloodSerial,float waterStrength,float bloodStrength,
    const StorylandScreenDropletCamera& camera);
bool captureFrame(std::vector<uint8_t>& rgba, int& width, int& height, std::string& error);
void glBegin(GLenum mode);
void glEnd();
void glVertex2f(GLfloat x, GLfloat y);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex3fv(const GLfloat* p);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor3fv(const GLfloat* p);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glColor4fv(const GLfloat* p);
void glColor3ub(GLubyte r, GLubyte g, GLubyte b);
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void glNormal3f(GLfloat x, GLfloat y, GLfloat z);
void glTexCoord2f(GLfloat u, GLfloat v);
void glTexCoord4fv(const GLfloat* p);
void glEnable(GLenum cap);
void glDisable(GLenum cap);
void glEnableClientState(GLenum cap);
void glDisableClientState(GLenum cap);
void glClear(GLbitfield mask);
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
void glScissor(GLint x, GLint y, GLsizei w, GLsizei h);
void glMatrixMode(GLenum mode);
void glLoadIdentity();
void glLoadMatrixd(const GLdouble* p);
void glMultMatrixf(const GLfloat* p);
void glPushMatrix();
void glPopMatrix();
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z);
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void glPushAttrib(GLbitfield mask);
void glPopAttrib();
void glPushClientAttrib(GLbitfield mask);
void glPopClientAttrib();
void glGetIntegerv(GLenum key, GLint* p);
void glGetFloatv(GLenum key, GLfloat* p);
void glGetDoublev(GLenum key, GLdouble* p);
void glGetTexEnviv(GLenum target, GLenum key, GLint* p);
void glGetTexLevelParameteriv(GLenum target,GLint level,GLenum key,GLint* value);
void glGetTexParameteriv(GLenum target,GLenum key,GLint* value);
GLenum glGetError();
void glGenTextures(GLsizei n, GLuint* p);
void glDeleteTextures(GLsizei n, const GLuint* p);
void glBindTexture(GLenum target, GLuint id);
void glTexParameteri(GLenum target, GLenum key, GLint value);
void glTexEnvi(GLenum target, GLenum key, GLint value);
void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
void glTexSubImage2D(GLenum target,GLint level,GLint x,GLint y,GLsizei w,GLsizei h,
    GLenum format,GLenum type,const GLvoid* pixels);
void glPixelStorei(GLenum key, GLint value);
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* p);
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* p);
void glNormalPointer(GLenum type, GLsizei stride, const GLvoid* p);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* p);
void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
void glDepthFunc(GLenum value);
void glDepthMask(GLboolean value);
void glBlendFunc(GLenum src, GLenum dst);
void glAlphaFunc(GLenum func, GLclampf ref);
void glPolygonMode(GLenum face, GLenum mode);
void glPolygonOffset(GLfloat factor, GLfloat units);
void glCullFace(GLenum face);
void glFrontFace(GLenum winding);
void glLineWidth(GLfloat value);
void glPointSize(GLfloat value);
void glShadeModel(GLenum value);
void glHint(GLenum target, GLenum mode);
void glFogf(GLenum key, GLfloat value);
void glFogi(GLenum key, GLint value);
void glFogfv(GLenum key, const GLfloat* p);
void glLightModelfv(GLenum key, const GLfloat* p);
void glLightfv(GLenum light, GLenum key, const GLfloat* p);
void glMaterialf(GLenum face, GLenum key, GLfloat value);
void glMaterialfv(GLenum face, GLenum key, const GLfloat* p);
void glColorMaterial(GLenum face, GLenum mode);
GLuint glGenLists(GLsizei range);
void glDeleteLists(GLuint list, GLsizei range);
void glListBase(GLuint base);
void glCallLists(GLsizei n, GLenum type, const GLvoid* p);
void glRasterPos2f(GLfloat x, GLfloat y);
void glRasterPos2i(GLint x, GLint y);
void glDrawPixels(GLsizei w, GLsizei h, GLenum format, GLenum type, const GLvoid* p);
void glFlush();
void glFinish();
HGLRC wglCreateContext(HDC dc);
BOOL wglDeleteContext(HGLRC context);
BOOL wglMakeCurrent(HDC dc, HGLRC context);
HGLRC wglGetCurrentContext();
HDC wglGetCurrentDC();
BOOL wglUseFontBitmapsW(HDC dc, DWORD first, DWORD count, DWORD base);
#else
inline void setGraphicsQuality(const StorylandGraphicsQualitySettings&,bool) {}
inline float maximumAnisotropy() {return 1.0f;}
inline void drawFxaa() {}
inline bool active() { return false; }
inline bool requested() { return false; }
inline void selectFromCommandLine() {}
inline const std::string& error() { static const std::string value; return value; }
inline bool beginSurface(bool, StorylandShaderKind, bool, int, int, const float*, const float*, const float*,
    const float*,float,float,int=-1,const float* =nullptr,const float* =nullptr,const float* =nullptr,
    float=1.0f,float=1.0f) { return false; }
inline void setUnderwater(const StorylandUnderwaterFrame&) {}
inline void drawUnderwater(const StorylandUnderwaterFrame&) {}
inline void drawPs2PostEffects(const StorylandPs2PostEffects&) {}
inline void drawScreenDroplets(bool,float,std::uint32_t,std::uint32_t,float,float,
    const StorylandScreenDropletCamera&) {}
inline void endSurface() {}
inline bool beginWater(const WaterSettings&) {return false;}
inline void endWater() {}
inline void setMaterialEffect(float) {}
inline void setPrelightScale(float) {}
inline void setPspStripState(int,float,float) {}
inline void setPspPrelitModel(bool) {}
inline void setPspLocalLight(unsigned,const float*,const float*) {}
inline void setPs2Lighting(const StorylandPs2LightingColors&,const StorylandPs2FogParameters&) {}
inline void setPs2PrelitModel(bool) {}
inline void setPs2LocalLight(unsigned,const float*,const float*) {}
inline void setPs2EnvironmentCoefficient(float) {}
inline void setPs2BudokaiReflection(bool) {}
inline void setPs2EnvironmentBasis(const float*,const float*) {}
inline void setSecondaryUv(float,float) {}
inline void setDualTexture(GLuint,int,float,const float* =nullptr,int =64,float =1.0f,float =1.0f,int =0) {}
inline GLuint beginVehicleEnvironmentCapture(int,std::string&) {return 0;}
inline void endVehicleEnvironmentCapture() {}
inline void releaseVehicleEnvironmentCapture() {}
inline GLuint vehicleEnvironmentTexture() {return 0;}
#endif
}
#if defined(STORYLAND_ENABLE_DIRECTX) && !defined(STORYLAND_DIRECTX_IMPLEMENTATION)
inline void storyland_glBegin(GLenum mode) { if(storylandDx::active()) return storylandDx::glBegin(mode); return ::glBegin(mode); }
#define glBegin storyland_glBegin
inline void storyland_glEnd() { if(storylandDx::active()) return storylandDx::glEnd(); return ::glEnd(); }
#define glEnd storyland_glEnd
inline void storyland_glVertex2f(GLfloat x, GLfloat y) { if(storylandDx::active()) return storylandDx::glVertex2f(x,y); return ::glVertex2f(x,y); }
#define glVertex2f storyland_glVertex2f
inline void storyland_glVertex3f(GLfloat x, GLfloat y, GLfloat z) { if(storylandDx::active()) return storylandDx::glVertex3f(x,y,z); return ::glVertex3f(x,y,z); }
#define glVertex3f storyland_glVertex3f
inline void storyland_glVertex3fv(const GLfloat* p) { if(storylandDx::active()) return storylandDx::glVertex3fv(p); return ::glVertex3fv(p); }
#define glVertex3fv storyland_glVertex3fv
inline void storyland_glColor3f(GLfloat r, GLfloat g, GLfloat b) { if(storylandDx::active()) return storylandDx::glColor3f(r,g,b); return ::glColor3f(r,g,b); }
#define glColor3f storyland_glColor3f
inline void storyland_glColor3fv(const GLfloat* p) { if(storylandDx::active()) return storylandDx::glColor3fv(p); return ::glColor3fv(p); }
#define glColor3fv storyland_glColor3fv
inline void storyland_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { if(storylandDx::active()) return storylandDx::glColor4f(r,g,b,a); return ::glColor4f(r,g,b,a); }
#define glColor4f storyland_glColor4f
inline void storyland_glColor4fv(const GLfloat* p) { if(storylandDx::active()) return storylandDx::glColor4fv(p); return ::glColor4fv(p); }
#define glColor4fv storyland_glColor4fv
inline void storyland_glColor3ub(GLubyte r, GLubyte g, GLubyte b) { if(storylandDx::active()) return storylandDx::glColor3ub(r,g,b); return ::glColor3ub(r,g,b); }
#define glColor3ub storyland_glColor3ub
inline void storyland_glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { if(storylandDx::active()) return storylandDx::glColor4ub(r,g,b,a); return ::glColor4ub(r,g,b,a); }
#define glColor4ub storyland_glColor4ub
inline void storyland_glNormal3f(GLfloat x, GLfloat y, GLfloat z) { if(storylandDx::active()) return storylandDx::glNormal3f(x,y,z); return ::glNormal3f(x,y,z); }
#define glNormal3f storyland_glNormal3f
inline void storyland_glTexCoord2f(GLfloat u, GLfloat v) { if(storylandDx::active()) return storylandDx::glTexCoord2f(u,v); return ::glTexCoord2f(u,v); }
#define glTexCoord2f storyland_glTexCoord2f
inline void storyland_glTexCoord4fv(const GLfloat* p) { if(storylandDx::active()) return storylandDx::glTexCoord4fv(p); return ::glTexCoord4fv(p); }
#define glTexCoord4fv storyland_glTexCoord4fv
inline void storyland_glEnable(GLenum cap) { if(storylandDx::active()) return storylandDx::glEnable(cap); return ::glEnable(cap); }
#define glEnable storyland_glEnable
inline void storyland_glDisable(GLenum cap) { if(storylandDx::active()) return storylandDx::glDisable(cap); return ::glDisable(cap); }
#define glDisable storyland_glDisable
inline void storyland_glEnableClientState(GLenum cap) { if(storylandDx::active()) return storylandDx::glEnableClientState(cap); return ::glEnableClientState(cap); }
#define glEnableClientState storyland_glEnableClientState
inline void storyland_glDisableClientState(GLenum cap) { if(storylandDx::active()) return storylandDx::glDisableClientState(cap); return ::glDisableClientState(cap); }
#define glDisableClientState storyland_glDisableClientState
inline void storyland_glClear(GLbitfield mask) { if(storylandDx::active()) return storylandDx::glClear(mask); return ::glClear(mask); }
#define glClear storyland_glClear
inline void storyland_glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { if(storylandDx::active()) return storylandDx::glClearColor(r,g,b,a); return ::glClearColor(r,g,b,a); }
#define glClearColor storyland_glClearColor
inline void storyland_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { if(storylandDx::active()) return storylandDx::glViewport(x,y,w,h); return ::glViewport(x,y,w,h); }
#define glViewport storyland_glViewport
inline void storyland_glScissor(GLint x, GLint y, GLsizei w, GLsizei h) { if(storylandDx::active()) return storylandDx::glScissor(x,y,w,h); return ::glScissor(x,y,w,h); }
#define glScissor storyland_glScissor
inline void storyland_glMatrixMode(GLenum mode) { if(storylandDx::active()) return storylandDx::glMatrixMode(mode); return ::glMatrixMode(mode); }
#define glMatrixMode storyland_glMatrixMode
inline void storyland_glLoadIdentity() { if(storylandDx::active()) return storylandDx::glLoadIdentity(); return ::glLoadIdentity(); }
#define glLoadIdentity storyland_glLoadIdentity
inline void storyland_glLoadMatrixd(const GLdouble* p) { if(storylandDx::active()) return storylandDx::glLoadMatrixd(p); return ::glLoadMatrixd(p); }
#define glLoadMatrixd storyland_glLoadMatrixd
inline void storyland_glMultMatrixf(const GLfloat* p) { if(storylandDx::active()) return storylandDx::glMultMatrixf(p); return ::glMultMatrixf(p); }
#define glMultMatrixf storyland_glMultMatrixf
inline void storyland_glPushMatrix() { if(storylandDx::active()) return storylandDx::glPushMatrix(); return ::glPushMatrix(); }
#define glPushMatrix storyland_glPushMatrix
inline void storyland_glPopMatrix() { if(storylandDx::active()) return storylandDx::glPopMatrix(); return ::glPopMatrix(); }
#define glPopMatrix storyland_glPopMatrix
inline void storyland_glTranslatef(GLfloat x, GLfloat y, GLfloat z) { if(storylandDx::active()) return storylandDx::glTranslatef(x,y,z); return ::glTranslatef(x,y,z); }
#define glTranslatef storyland_glTranslatef
inline void storyland_glScalef(GLfloat x, GLfloat y, GLfloat z) { if(storylandDx::active()) return storylandDx::glScalef(x,y,z); return ::glScalef(x,y,z); }
#define glScalef storyland_glScalef
inline void storyland_glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z) { if(storylandDx::active()) return storylandDx::glRotatef(a,x,y,z); return ::glRotatef(a,x,y,z); }
#define glRotatef storyland_glRotatef
inline void storyland_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { if(storylandDx::active()) return storylandDx::glOrtho(l,r,b,t,n,f); return ::glOrtho(l,r,b,t,n,f); }
#define glOrtho storyland_glOrtho
inline void storyland_glPushAttrib(GLbitfield mask) { if(storylandDx::active()) return storylandDx::glPushAttrib(mask); return ::glPushAttrib(mask); }
#define glPushAttrib storyland_glPushAttrib
inline void storyland_glPopAttrib() { if(storylandDx::active()) return storylandDx::glPopAttrib(); return ::glPopAttrib(); }
#define glPopAttrib storyland_glPopAttrib
inline void storyland_glPushClientAttrib(GLbitfield mask) { if(storylandDx::active()) return storylandDx::glPushClientAttrib(mask); return ::glPushClientAttrib(mask); }
#define glPushClientAttrib storyland_glPushClientAttrib
inline void storyland_glPopClientAttrib() { if(storylandDx::active()) return storylandDx::glPopClientAttrib(); return ::glPopClientAttrib(); }
#define glPopClientAttrib storyland_glPopClientAttrib
inline void storyland_glGetIntegerv(GLenum key, GLint* p) { if(storylandDx::active()) return storylandDx::glGetIntegerv(key,p); return ::glGetIntegerv(key,p); }
#define glGetIntegerv storyland_glGetIntegerv
inline void storyland_glGetFloatv(GLenum key, GLfloat* p) { if(storylandDx::active()) return storylandDx::glGetFloatv(key,p); return ::glGetFloatv(key,p); }
#define glGetFloatv storyland_glGetFloatv
inline void storyland_glGetDoublev(GLenum key, GLdouble* p) { if(storylandDx::active()) return storylandDx::glGetDoublev(key,p); return ::glGetDoublev(key,p); }
#define glGetDoublev storyland_glGetDoublev
inline void storyland_glGetTexEnviv(GLenum target, GLenum key, GLint* p) { if(storylandDx::active()) return storylandDx::glGetTexEnviv(target,key,p); return ::glGetTexEnviv(target,key,p); }
#define glGetTexEnviv storyland_glGetTexEnviv
inline void storyland_glGetTexLevelParameteriv(GLenum target,GLint level,GLenum key,GLint* value) { if(storylandDx::active()) return storylandDx::glGetTexLevelParameteriv(target,level,key,value); return ::glGetTexLevelParameteriv(target,level,key,value); }
#define glGetTexLevelParameteriv storyland_glGetTexLevelParameteriv
inline void storyland_glGetTexParameteriv(GLenum target,GLenum key,GLint* value) { if(storylandDx::active()) return storylandDx::glGetTexParameteriv(target,key,value); return ::glGetTexParameteriv(target,key,value); }
#define glGetTexParameteriv storyland_glGetTexParameteriv
inline GLenum storyland_glGetError() { if(storylandDx::active()) return storylandDx::glGetError(); return ::glGetError(); }
#define glGetError storyland_glGetError
inline void storyland_glGenTextures(GLsizei n, GLuint* p) { if(storylandDx::active()) return storylandDx::glGenTextures(n,p); return ::glGenTextures(n,p); }
#define glGenTextures storyland_glGenTextures
inline void storyland_glDeleteTextures(GLsizei n, const GLuint* p) { if(storylandDx::active()) return storylandDx::glDeleteTextures(n,p); return ::glDeleteTextures(n,p); }
#define glDeleteTextures storyland_glDeleteTextures
inline void storyland_glBindTexture(GLenum target, GLuint id) { if(storylandDx::active()) return storylandDx::glBindTexture(target,id); return ::glBindTexture(target,id); }
#define glBindTexture storyland_glBindTexture
inline void storyland_glTexParameteri(GLenum target, GLenum key, GLint value) { if(storylandDx::active()) return storylandDx::glTexParameteri(target,key,value); return ::glTexParameteri(target,key,value); }
#define glTexParameteri storyland_glTexParameteri
inline void storyland_glTexEnvi(GLenum target, GLenum key, GLint value) { if(storylandDx::active()) return storylandDx::glTexEnvi(target,key,value); return ::glTexEnvi(target,key,value); }
#define glTexEnvi storyland_glTexEnvi
inline void storyland_glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const GLvoid* pixels) { if(storylandDx::active()) return storylandDx::glTexImage2D(target,level,internal,w,h,border,format,type,pixels); return ::glTexImage2D(target,level,internal,w,h,border,format,type,pixels); }
#define glTexImage2D storyland_glTexImage2D
inline void storyland_glTexSubImage2D(GLenum target,GLint level,GLint x,GLint y,GLsizei w,GLsizei h,GLenum format,GLenum type,const GLvoid* pixels) { if(storylandDx::active()) return storylandDx::glTexSubImage2D(target,level,x,y,w,h,format,type,pixels); return ::glTexSubImage2D(target,level,x,y,w,h,format,type,pixels); }
#define glTexSubImage2D storyland_glTexSubImage2D
inline void storyland_glPixelStorei(GLenum key, GLint value) { if(storylandDx::active()) return storylandDx::glPixelStorei(key,value); return ::glPixelStorei(key,value); }
#define glPixelStorei storyland_glPixelStorei
inline void storyland_glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* p) { if(storylandDx::active()) return storylandDx::glVertexPointer(size,type,stride,p); return ::glVertexPointer(size,type,stride,p); }
#define glVertexPointer storyland_glVertexPointer
inline void storyland_glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* p) { if(storylandDx::active()) return storylandDx::glColorPointer(size,type,stride,p); return ::glColorPointer(size,type,stride,p); }
#define glColorPointer storyland_glColorPointer
inline void storyland_glNormalPointer(GLenum type, GLsizei stride, const GLvoid* p) { if(storylandDx::active()) return storylandDx::glNormalPointer(type,stride,p); return ::glNormalPointer(type,stride,p); }
#define glNormalPointer storyland_glNormalPointer
inline void storyland_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* p) { if(storylandDx::active()) return storylandDx::glTexCoordPointer(size,type,stride,p); return ::glTexCoordPointer(size,type,stride,p); }
#define glTexCoordPointer storyland_glTexCoordPointer
inline void storyland_glDrawArrays(GLenum mode, GLint first, GLsizei count) { if(storylandDx::active()) return storylandDx::glDrawArrays(mode,first,count); return ::glDrawArrays(mode,first,count); }
#define glDrawArrays storyland_glDrawArrays
inline void storyland_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) { if(storylandDx::active()) return storylandDx::glDrawElements(mode,count,type,indices); return ::glDrawElements(mode,count,type,indices); }
#define glDrawElements storyland_glDrawElements
inline void storyland_glDepthFunc(GLenum value) { if(storylandDx::active()) return storylandDx::glDepthFunc(value); return ::glDepthFunc(value); }
#define glDepthFunc storyland_glDepthFunc
inline void storyland_glDepthMask(GLboolean value) { if(storylandDx::active()) return storylandDx::glDepthMask(value); return ::glDepthMask(value); }
#define glDepthMask storyland_glDepthMask
inline void storyland_glBlendFunc(GLenum src, GLenum dst) { if(storylandDx::active()) return storylandDx::glBlendFunc(src,dst); return ::glBlendFunc(src,dst); }
#define glBlendFunc storyland_glBlendFunc
inline void storyland_glAlphaFunc(GLenum func, GLclampf ref) { if(storylandDx::active()) return storylandDx::glAlphaFunc(func,ref); return ::glAlphaFunc(func,ref); }
#define glAlphaFunc storyland_glAlphaFunc
inline void storyland_glPolygonMode(GLenum face, GLenum mode) { if(storylandDx::active()) return storylandDx::glPolygonMode(face,mode); return ::glPolygonMode(face,mode); }
#define glPolygonMode storyland_glPolygonMode
inline void storyland_glPolygonOffset(GLfloat factor, GLfloat units) { if(storylandDx::active()) return storylandDx::glPolygonOffset(factor,units); return ::glPolygonOffset(factor,units); }
#define glPolygonOffset storyland_glPolygonOffset
inline void storyland_glCullFace(GLenum face) { if(storylandDx::active()) return storylandDx::glCullFace(face); return ::glCullFace(face); }
#define glCullFace storyland_glCullFace
inline void storyland_glFrontFace(GLenum winding) { if(storylandDx::active()) return storylandDx::glFrontFace(winding); return ::glFrontFace(winding); }
#define glFrontFace storyland_glFrontFace
inline void storyland_glLineWidth(GLfloat value) { if(storylandDx::active()) return storylandDx::glLineWidth(value); return ::glLineWidth(value); }
#define glLineWidth storyland_glLineWidth
inline void storyland_glPointSize(GLfloat value) { if(storylandDx::active()) return storylandDx::glPointSize(value); return ::glPointSize(value); }
#define glPointSize storyland_glPointSize
inline void storyland_glShadeModel(GLenum value) { if(storylandDx::active()) return storylandDx::glShadeModel(value); return ::glShadeModel(value); }
#define glShadeModel storyland_glShadeModel
inline void storyland_glHint(GLenum target, GLenum mode) { if(storylandDx::active()) return storylandDx::glHint(target,mode); return ::glHint(target,mode); }
#define glHint storyland_glHint
inline void storyland_glFogf(GLenum key, GLfloat value) { if(storylandDx::active()) return storylandDx::glFogf(key,value); return ::glFogf(key,value); }
#define glFogf storyland_glFogf
inline void storyland_glFogi(GLenum key, GLint value) { if(storylandDx::active()) return storylandDx::glFogi(key,value); return ::glFogi(key,value); }
#define glFogi storyland_glFogi
inline void storyland_glFogfv(GLenum key, const GLfloat* p) { if(storylandDx::active()) return storylandDx::glFogfv(key,p); return ::glFogfv(key,p); }
#define glFogfv storyland_glFogfv
inline void storyland_glLightModelfv(GLenum key, const GLfloat* p) { if(storylandDx::active()) return storylandDx::glLightModelfv(key,p); return ::glLightModelfv(key,p); }
#define glLightModelfv storyland_glLightModelfv
inline void storyland_glLightfv(GLenum light, GLenum key, const GLfloat* p) { if(storylandDx::active()) return storylandDx::glLightfv(light,key,p); return ::glLightfv(light,key,p); }
#define glLightfv storyland_glLightfv
inline void storyland_glMaterialf(GLenum face, GLenum key, GLfloat value) { if(storylandDx::active()) return storylandDx::glMaterialf(face,key,value); return ::glMaterialf(face,key,value); }
#define glMaterialf storyland_glMaterialf
inline void storyland_glMaterialfv(GLenum face, GLenum key, const GLfloat* p) { if(storylandDx::active()) return storylandDx::glMaterialfv(face,key,p); return ::glMaterialfv(face,key,p); }
#define glMaterialfv storyland_glMaterialfv
inline void storyland_glColorMaterial(GLenum face, GLenum mode) { if(storylandDx::active()) return storylandDx::glColorMaterial(face,mode); return ::glColorMaterial(face,mode); }
#define glColorMaterial storyland_glColorMaterial
inline GLuint storyland_glGenLists(GLsizei range) { if(storylandDx::active()) return storylandDx::glGenLists(range); return ::glGenLists(range); }
#define glGenLists storyland_glGenLists
inline void storyland_glDeleteLists(GLuint list, GLsizei range) { if(storylandDx::active()) return storylandDx::glDeleteLists(list,range); return ::glDeleteLists(list,range); }
#define glDeleteLists storyland_glDeleteLists
inline void storyland_glListBase(GLuint base) { if(storylandDx::active()) return storylandDx::glListBase(base); return ::glListBase(base); }
#define glListBase storyland_glListBase
inline void storyland_glCallLists(GLsizei n, GLenum type, const GLvoid* p) { if(storylandDx::active()) return storylandDx::glCallLists(n,type,p); return ::glCallLists(n,type,p); }
#define glCallLists storyland_glCallLists
inline void storyland_glRasterPos2f(GLfloat x, GLfloat y) { if(storylandDx::active()) return storylandDx::glRasterPos2f(x,y); return ::glRasterPos2f(x,y); }
#define glRasterPos2f storyland_glRasterPos2f
inline void storyland_glRasterPos2i(GLint x, GLint y) { if(storylandDx::active()) return storylandDx::glRasterPos2i(x,y); return ::glRasterPos2i(x,y); }
#define glRasterPos2i storyland_glRasterPos2i
inline void storyland_glDrawPixels(GLsizei w, GLsizei h, GLenum format, GLenum type, const GLvoid* p) { if(storylandDx::active()) return storylandDx::glDrawPixels(w,h,format,type,p); return ::glDrawPixels(w,h,format,type,p); }
#define glDrawPixels storyland_glDrawPixels
inline void storyland_glFlush() { if(storylandDx::active()) return storylandDx::glFlush(); return ::glFlush(); }
#define glFlush storyland_glFlush
inline void storyland_glFinish() { if(storylandDx::active()) return storylandDx::glFinish(); return ::glFinish(); }
#define glFinish storyland_glFinish
inline HGLRC storyland_wglCreateContext(HDC dc) { if(storylandDx::requested()) return storylandDx::wglCreateContext(dc); return ::wglCreateContext(dc); }
#define wglCreateContext storyland_wglCreateContext
inline BOOL storyland_wglDeleteContext(HGLRC context) { if(storylandDx::active()) return storylandDx::wglDeleteContext(context); return ::wglDeleteContext(context); }
#define wglDeleteContext storyland_wglDeleteContext
inline BOOL storyland_wglMakeCurrent(HDC dc, HGLRC context) { if(storylandDx::active()) return storylandDx::wglMakeCurrent(dc,context); return ::wglMakeCurrent(dc,context); }
#define wglMakeCurrent storyland_wglMakeCurrent
inline HGLRC storyland_wglGetCurrentContext() { if(storylandDx::active()) return storylandDx::wglGetCurrentContext(); return ::wglGetCurrentContext(); }
#define wglGetCurrentContext storyland_wglGetCurrentContext
inline HDC storyland_wglGetCurrentDC() { if(storylandDx::active()) return storylandDx::wglGetCurrentDC(); return ::wglGetCurrentDC(); }
#define wglGetCurrentDC storyland_wglGetCurrentDC
inline BOOL storyland_wglUseFontBitmapsW(HDC dc, DWORD first, DWORD count, DWORD base) { if(storylandDx::active()) return storylandDx::wglUseFontBitmapsW(dc,first,count,base); return ::wglUseFontBitmapsW(dc,first,count,base); }
#define wglUseFontBitmapsW storyland_wglUseFontBitmapsW
inline BOOL storylandSwapBuffers(HDC dc) { return storylandDx::active()?storylandDx::present(dc) : ::SwapBuffers(dc); }
#define SwapBuffers storylandSwapBuffers
#endif
