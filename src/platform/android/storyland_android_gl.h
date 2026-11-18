#pragma once
#include <GLES3/gl3.h>
#include <string>
using GLdouble = double;
constexpr GLenum GL_MODELVIEW = 0x1700, GL_PROJECTION = 0x1701,
                 GL_MODELVIEW_MATRIX = 0x0BA6, GL_PROJECTION_MATRIX = 0x0BA7,
                 GL_TEXTURE_MATRIX = 0x0BA8, GL_MATRIX_MODE = 0x0BA0,
                 GL_CURRENT_COLOR = 0x0B00, GL_CLIENT_ACTIVE_TEXTURE = 0x84E1,
                 GL_MAX_TEXTURE_UNITS = 0x84E2;
constexpr GLenum GL_ALPHA_TEST = 0x0BC0, GL_LIGHTING = 0x0B50, GL_FOG = 0x0B60,
                 GL_QUADS = 7, GL_QUAD_STRIP = 8;
constexpr GLenum GL_VERTEX_ARRAY = 0x8074, GL_COLOR_ARRAY = 0x8076,
                 GL_TEXTURE_COORD_ARRAY = 0x8078, GL_NORMAL_ARRAY = 0x8075;
constexpr GLenum GL_TEXTURE_ENV = 0x2300, GL_TEXTURE_ENV_MODE = 0x2200,
                 GL_MODULATE = 0x2100, GL_SMOOTH = 0x1D01;
constexpr GLbitfield GL_ENABLE_BIT = 0x2000, GL_CURRENT_BIT = 1,
                     GL_LINE_BIT = 4, GL_POLYGON_BIT = 8,
                     GL_TRANSFORM_BIT = 0x1000, GL_VIEWPORT_BIT = 0x800,
                     GL_POINT_BIT = 2;
constexpr GLbitfield GL_LIGHTING_BIT = 0x40, GL_TEXTURE_BIT = 0x40000;
constexpr GLbitfield GL_ALL_ATTRIB_BITS = 0x000FFFFF, GL_CLIENT_VERTEX_ARRAY_BIT = 2;
bool storylandAndroidGlInitialize(std::string &error);
void storylandAndroidGlShutdown();
std::string storylandAndroidShader(const char *source, bool vertex);
void storylandGlUseProgram(GLuint program);
void storylandGlEnable(GLenum value);
void storylandGlDisable(GLenum value);
void storylandGlDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
void storylandGlDrawArrays(GLenum mode, GLint first, GLsizei count);
void storylandGlGetIntegerv(GLenum name, GLint *values);
void storylandGlGetFloatv(GLenum name, GLfloat *values);
void glMatrixMode(GLenum mode);
void glLoadIdentity();
void glLoadMatrixf(const GLfloat *matrix);
void glMultMatrixf(const GLfloat *matrix);
void glPushMatrix();
void glPopMatrix();
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glOrtho(double left, double right, double bottom, double top,
             double nearValue, double farValue);
void glFrustum(double left, double right, double bottom, double top,
               double nearValue, double farValue);
void glGetDoublev(GLenum mode, GLdouble *values);
void glBegin(GLenum mode);
void glEnd();
void glVertex3f(float x, float y, float z);
inline void glVertex3fv(const GLfloat *value) { glVertex3f(value[0], value[1], value[2]); }
void glVertex2f(float x, float y);
void glColor4f(float r, float g, float b, float a);
inline void glColor4fv(const GLfloat *value) { glColor4f(value[0], value[1], value[2], value[3]); }
inline void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
  glColor4f(red / 255.f, green / 255.f, blue / 255.f, alpha / 255.f);
}
inline void glColor4ubv(const GLubyte *value) {
  glColor4ub(value[0], value[1], value[2], value[3]);
}
void glNormal3f(float x, float y, float z);
void glTexCoord2f(float u, float v);
inline void glTexCoord2fv(const GLfloat *value) { glTexCoord2f(value[0], value[1]); }
void glNormalPointer(GLenum type, GLsizei stride, const void *pointer);
void glVertexPointer(GLint size, GLenum type, GLsizei stride,
                     const void *pointer);
void glColorPointer(GLint size, GLenum type, GLsizei stride,
                    const void *pointer);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride,
                       const void *pointer);
void glEnableClientState(GLenum state);
void glDisableClientState(GLenum state);
void glClientActiveTexture(GLenum unit);
void glAlphaFunc(GLenum function, float value);
void glPushAttrib(GLbitfield bits);
void glPopAttrib();
void glPushClientAttrib(GLbitfield bits);
void glPopClientAttrib();
void glPointSize(float value);
inline void glTexEnvi(GLenum, GLenum, GLint) {}
inline void glShadeModel(GLenum) {}
#ifndef STORYLAND_ANDROID_GL_IMPLEMENTATION
#define glUseProgram storylandGlUseProgram
#define glEnable storylandGlEnable
#define glDisable storylandGlDisable
#define glDrawArrays storylandGlDrawArrays
#define glDrawElements storylandGlDrawElements
#define glGetIntegerv storylandGlGetIntegerv
#define glGetFloatv storylandGlGetFloatv
#endif
