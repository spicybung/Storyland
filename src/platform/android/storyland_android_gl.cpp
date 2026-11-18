#define STORYLAND_ANDROID_GL_IMPLEMENTATION
#include "src/platform/android/storyland_android_gl.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>
#include <limits>
#include <cstdint>

namespace {
using Matrix = std::array<float, 16>;
Matrix identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }
Matrix multiply(const Matrix &a, const Matrix &b) {
  Matrix result{};
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 4; ++row)
      for (int k = 0; k < 4; ++k)
        result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
  return result;
}
struct Vertex {
  float position[4], color[4], uv[2], uv1[2], normal[3];
};
struct Attribute {
  const unsigned char *pointer = nullptr;
  int size = 0, stride = 0;
  GLenum type = GL_FLOAT;
  bool enabled = false;
};
struct SavedState {
  GLbitfield bits = 0;
  bool texture = false, alpha = false;
  GLboolean depth = false, blend = false, cull = false, scissor = false, writeDepth = true;
  GLint source = GL_ONE, destination = GL_ZERO, sourceAlpha = GL_ONE, destinationAlpha = GL_ZERO;
  GLint blendRgb = GL_FUNC_ADD, blendAlpha = GL_FUNC_ADD, depthFunction = GL_LESS;
  GLint cullFace = GL_BACK, frontFace = GL_CCW, activeTexture = GL_TEXTURE0;
  float line = 1, alphaLimit = 0, pointSize = 1, clearDepth = 1;
  GLenum alphaFunction = GL_GREATER;
  GLenum matrix = GL_MODELVIEW;
  std::array<float, 4> color{1, 1, 1, 1};
  std::array<GLfloat, 4> clearColor{};
  std::array<GLint, 4> viewport{}, scissorBox{};
  std::array<GLboolean, 4> writeColor{GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
  std::array<GLint, 2> textures{}, cubes{};
};
struct SavedClientState {
  Attribute position, color, normal;
  std::array<Attribute, 2> uv;
  unsigned textureUnit = 0;
  GLint vertexArray = 0, arrayBuffer = 0, elementBuffer = 0;
};
struct Context {
  GLuint program = 0, fixedProgram = 0, buffer = 0, elementBuffer = 0, vao = 0;
  Matrix model = identity(), projection = identity(), textureMatrix = identity();
  std::vector<Matrix> modelStack, projectionStack, textureStack;
  std::vector<SavedState> states;
  std::vector<SavedClientState> clientStates;
  GLenum matrixMode = GL_MODELVIEW, primitive = GL_TRIANGLES;
  unsigned textureUnit = 0;
  Attribute position, color, uv[2], normal;
  Vertex current{{0, 0, 0, 1}, {1, 1, 1, 1}, {0, 0}, {0, 0}, {0, 0, 1}};
  bool texture = false, alpha = false;
  float alphaLimit = 0, pointSize = 1;
  GLenum alphaFunction = GL_GREATER;
  std::vector<Vertex> immediate;
};
Context context;
Matrix &currentMatrix() {
  if (context.matrixMode == GL_PROJECTION) return context.projection;
  if (context.matrixMode == GL_TEXTURE) return context.textureMatrix;
  return context.model;
}
std::vector<Matrix>& currentMatrixStack() {
  if (context.matrixMode == GL_PROJECTION) return context.projectionStack;
  if (context.matrixMode == GL_TEXTURE) return context.textureStack;
  return context.modelStack;
}
void replace(std::string &value, const std::string &from,
             const std::string &to) {
  size_t at = 0;
  while ((at = value.find(from, at)) != std::string::npos) {
    value.replace(at, from.size(), to);
    at += to.size();
  }
}
GLuint shader(GLenum type, const char *source, std::string &error) {
  GLuint value = glCreateShader(type);
  glShaderSource(value, 1, &source, nullptr);
  glCompileShader(value);
  GLint okay = 0;
  glGetShaderiv(value, GL_COMPILE_STATUS, &okay);
  if (!okay) {
    char log[2048]{};
    glGetShaderInfoLog(value, sizeof(log), nullptr, log);
    error = log;
    glDeleteShader(value);
    return 0;
  }
  return value;
}
void draw(GLenum mode, std::vector<Vertex> &vertices, const std::vector<uint32_t>* indices = nullptr) {
  if (vertices.empty())
    return;
  if (mode == GL_QUADS || mode == GL_QUAD_STRIP) {
    std::vector<Vertex> triangles;
    if (mode == GL_QUADS)
      for (size_t i = 0; i + 3 < vertices.size(); i += 4)
        for (size_t j : {i, i + 1, i + 2, i, i + 2, i + 3})
          triangles.push_back(vertices[j]);
    else
      for (size_t i = 0; i + 3 < vertices.size(); i += 2)
        for (size_t j : {i, i + 1, i + 3, i, i + 3, i + 2})
          triangles.push_back(vertices[j]);
    vertices.swap(triangles);
    mode = GL_TRIANGLES;
  }
  const GLuint program =
      context.program ? context.program : context.fixedProgram;
  glUseProgram(program);
  const auto mvp = multiply(context.projection, context.model);
  glUniformMatrix4fv(glGetUniformLocation(program, "storyMvp"), 1, GL_FALSE,
                     mvp.data());
  glUniformMatrix4fv(glGetUniformLocation(program, "storyProjection"), 1,
                     GL_FALSE, context.projection.data());
  glUniformMatrix4fv(glGetUniformLocation(program, "storyModelView"), 1,
                     GL_FALSE, context.model.data());
  const auto &m = context.model;
  const float determinant = m[0] * (m[5] * m[10] - m[9] * m[6]) -
                            m[4] * (m[1] * m[10] - m[9] * m[2]) +
                            m[8] * (m[1] * m[6] - m[5] * m[2]);
  std::array<float, 9> normal{1, 0, 0, 0, 1, 0, 0, 0, 1};
  if (std::fabs(determinant) > 1e-10f)
    normal = {(m[5] * m[10] - m[9] * m[6]) / determinant,
              (m[8] * m[6] - m[4] * m[10]) / determinant,
              (m[4] * m[9] - m[8] * m[5]) / determinant,
              (m[9] * m[2] - m[1] * m[10]) / determinant,
              (m[0] * m[10] - m[8] * m[2]) / determinant,
              (m[8] * m[1] - m[0] * m[9]) / determinant,
              (m[1] * m[6] - m[5] * m[2]) / determinant,
              (m[4] * m[2] - m[0] * m[6]) / determinant,
              (m[0] * m[5] - m[4] * m[1]) / determinant};
  glUniformMatrix3fv(glGetUniformLocation(program, "storyNormal"), 1, GL_FALSE,
                     normal.data());
  glUniform1i(glGetUniformLocation(program, "storyAlphaTest"),
              context.alpha ? 1 : 0);
  glUniform1f(glGetUniformLocation(program, "storyAlphaLimit"),
              context.alphaLimit);
  glUniform1i(glGetUniformLocation(program, "storyAlphaFunction"),
              int(context.alphaFunction));
  glUniform1f(glGetUniformLocation(program, "storyPointSize"),
              context.pointSize);
  if (!context.program) {
    glUniform1i(glGetUniformLocation(program, "storyTexture"),
                context.texture ? 1 : 0);
    glUniform1i(glGetUniformLocation(program, "tex0"), 0);
  }
  glBindVertexArray(context.vao);
  glBindBuffer(GL_ARRAY_BUFFER, context.buffer);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(Vertex)),
               vertices.data(), GL_STREAM_DRAW);
  for (GLuint i = 0; i < 5; ++i)
    glEnableVertexAttribArray(i);
  glVertexAttribPointer(
      0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
      reinterpret_cast<const void *>(offsetof(Vertex, position)));
  glVertexAttribPointer(
      1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
      reinterpret_cast<const void *>(offsetof(Vertex, color)));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<const void *>(offsetof(Vertex, uv)));
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<const void *>(offsetof(Vertex, uv1)));
  glVertexAttribPointer(
      4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
      reinterpret_cast<const void *>(offsetof(Vertex, normal)));
  if (indices) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.elementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(indices->size() * sizeof(uint32_t)), indices->data(), GL_STREAM_DRAW);
    glDrawElements(mode, GLsizei(indices->size()), GL_UNSIGNED_INT, nullptr);
  } else glDrawArrays(mode, 0, GLsizei(vertices.size()));
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void copyAttribute(const Attribute &attribute, int index, float *target,
                   int maximum) {
  if (!attribute.enabled || !attribute.pointer)
    return;
  if (&attribute == &context.color && attribute.size == 3 && maximum == 4) target[3] = 1.0f;
  const auto pointer =
      attribute.pointer +
      size_t(index) *
          size_t(attribute.stride ? attribute.stride : attribute.size * (attribute.type == GL_UNSIGNED_BYTE ? 1 : 4));
  if (attribute.type == GL_UNSIGNED_BYTE) {
    for (int component = 0; component < std::min(attribute.size, maximum); ++component)
      target[component] = pointer[component] / 255.0f;
  } else std::memcpy(target, pointer, size_t(std::min(attribute.size, maximum)) * sizeof(float));
}
} // namespace

const char* storylandAndroidAlphaSource=R"GLSL(
uniform int storyAlphaTest;
uniform int storyAlphaFunction;
uniform float storyAlphaLimit;
bool storyAlphaPass(float alpha) {
    if(storyAlphaTest==0||storyAlphaFunction==519)return true;
    if(storyAlphaFunction==512)return false;
    if(storyAlphaFunction==513)return alpha<storyAlphaLimit;
    if(storyAlphaFunction==514)return alpha==storyAlphaLimit;
    if(storyAlphaFunction==515)return alpha<=storyAlphaLimit;
    if(storyAlphaFunction==516)return alpha>storyAlphaLimit;
    if(storyAlphaFunction==517)return alpha!=storyAlphaLimit;
    return alpha>=storyAlphaLimit;
}
)GLSL";

// Swap the desktop vertex inputs for the attributes filled by the GLES draw path.
std::string storylandAndroidShader(const char *source, bool vertex) {
  std::string body = source ? source : "";
  replace(body, "#version 120", "");
  replace(body, "#define FSIN varying", "#define FSIN in");
  replace(body, "#define texture texture2D", "");
  replace(body, "gl_ProjectionMatrix", "storyProjection");
  replace(body, "varying ", vertex ? "out " : "in ");
  replace(body, "texture2D(", "texture(");
  replace(body, "gl_ModelViewProjectionMatrix", "storyMvp");
  replace(body, "gl_ModelViewMatrix", "storyModelView");
  replace(body, "gl_NormalMatrix", "storyNormal");
  replace(body, "gl_MultiTexCoord0", "vec4(storyUv,0.0,1.0)");
  replace(body, "gl_MultiTexCoord1", "vec4(storyUv1,0.0,1.0)");
  replace(body, "gl_Vertex", "storyPosition");
  replace(body, "gl_Color", "storyColor");
  replace(body, "gl_Normal", "storyVertexNormal");
  replace(body, "gl_FragColor", "storyOutput");
  std::string header =
      "#version 300 es\nprecision highp float;\nprecision highp int;\n";
  if (vertex)
    header +=
        "layout(location=0) in vec4 storyPosition;\nlayout(location=1) in vec4 "
        "storyColor;\nlayout(location=2) in vec2 storyUv;\nlayout(location=3) "
        "in vec2 storyUv1;\nlayout(location=4) in vec3 "
        "storyVertexNormal;\nuniform mat4 storyMvp;\nuniform mat4 "
        "storyProjection;\nuniform mat4 "
        "storyModelView;\nuniform mat3 storyNormal;\n";
  else {
    header += "out vec4 storyOutput;\n";
    header += storylandAndroidAlphaSource;
    replace(body, "void main()", "void storyFragment()");
    replace(body, "main(void)", "storyFragment(void)");
    body += "\nvoid "
            "main(){storyFragment();if(!storyAlphaPass(storyOutput.a))discard;}\n";
  }
  return header + body;
}
bool storylandAndroidGlInitialize(std::string &error) {
  context = Context{};
  const char *vertex =
      "#version 300 es\nprecision highp float;layout(location=0) in vec4 "
      "p;layout(location=1) in vec4 c;layout(location=2) in vec2 u;uniform "
      "mat4 storyMvp;uniform float storyPointSize;out vec4 color;out vec2 "
      "uv;void "
      "main(){gl_Position=storyMvp*p;gl_PointSize=storyPointSize;color=c;uv=u;"
      "}";
  const std::string fragment = std::string(
      "#version 300 es\nprecision highp float;in vec4 color;in vec2 uv;uniform "
      "sampler2D tex0;uniform int storyTexture;out vec4 result;\n")+
      storylandAndroidAlphaSource+
      "void main(){result=color*(storyTexture!=0?texture(tex0,uv):vec4(1));"
      "if(!storyAlphaPass(result.a))discard;}";
  GLuint vs = shader(GL_VERTEX_SHADER, vertex, error),
         fs = shader(GL_FRAGMENT_SHADER, fragment.c_str(), error);
  if (!vs || !fs) {
    if (vs)
      glDeleteShader(vs);
    if (fs)
      glDeleteShader(fs);
    return false;
  }
  context.fixedProgram = glCreateProgram();
  glAttachShader(context.fixedProgram, vs);
  glAttachShader(context.fixedProgram, fs);
  glLinkProgram(context.fixedProgram);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint okay = 0;
  glGetProgramiv(context.fixedProgram, GL_LINK_STATUS, &okay);
  if (!okay) {
    error = "The OpenGL ES presentation shader could not link.";
    storylandAndroidGlShutdown();
    return false;
  }
  glGenBuffers(1, &context.buffer);
  glGenBuffers(1, &context.elementBuffer);
  glGenVertexArrays(1, &context.vao);
  return true;
}
void storylandAndroidGlShutdown() {
  glDeleteProgram(context.fixedProgram);
  glDeleteBuffers(1, &context.buffer);
  glDeleteBuffers(1, &context.elementBuffer);
  glDeleteVertexArrays(1, &context.vao);
  context = Context{};
}
void storylandGlUseProgram(GLuint program) {
  context.program = program;
  glUseProgram(program ? program : context.fixedProgram);
}
void storylandGlEnable(GLenum value) {
  if (value == GL_TEXTURE_2D) {
    GLint unit = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &unit);
    if (unit == GL_TEXTURE0) context.texture = true;
  }
  else if (value == GL_ALPHA_TEST)
    context.alpha = true;
  else if (value != GL_LIGHTING && value != GL_FOG)
    glEnable(value);
}
void storylandGlDisable(GLenum value) {
  if (value == GL_TEXTURE_2D) {
    GLint unit = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &unit);
    if (unit == GL_TEXTURE0) context.texture = false;
  }
  else if (value == GL_ALPHA_TEST)
    context.alpha = false;
  else if (value != GL_LIGHTING && value != GL_FOG)
    glDisable(value);
}
void glMatrixMode(GLenum mode) { context.matrixMode = mode; }
void glLoadIdentity() { currentMatrix() = identity(); }
void glLoadMatrixf(const GLfloat *matrix) {
  std::copy_n(matrix, 16, currentMatrix().begin());
}
void glMultMatrixf(const GLfloat *matrix) {
  Matrix value;
  std::copy(matrix, matrix + 16, value.begin());
  currentMatrix() = multiply(currentMatrix(), value);
}
void glPushMatrix() {
  auto &stack = currentMatrixStack();
  if (stack.size() < 64)
    stack.push_back(currentMatrix());
}
void glPopMatrix() {
  auto &stack = currentMatrixStack();
  if (!stack.empty()) {
    currentMatrix() = stack.back();
    stack.pop_back();
  }
}
void glTranslatef(float x, float y, float z) {
  auto m = identity();
  m[12] = x;
  m[13] = y;
  m[14] = z;
  glMultMatrixf(m.data());
}
void glScalef(float x, float y, float z) {
  auto m = identity();
  m[0] = x;
  m[5] = y;
  m[10] = z;
  glMultMatrixf(m.data());
}
void glRotatef(float angle, float x, float y, float z) {
  float length = std::hypot(x, y, z);
  if (length <= 0)
    return;
  x /= length;
  y /= length;
  z /= length;
  float c = std::cos(angle * .01745329252f),
        s = std::sin(angle * .01745329252f), t = 1 - c;
  Matrix m = {t * x * x + c,
              t * x * y + s * z,
              t * x * z - s * y,
              0,
              t * x * y - s * z,
              t * y * y + c,
              t * y * z + s * x,
              0,
              t * x * z + s * y,
              t * y * z - s * x,
              t * z * z + c,
              0,
              0,
              0,
              0,
              1};
  glMultMatrixf(m.data());
}
void glOrtho(double l, double r, double b, double t, double n, double f) {
  Matrix m{};
  m[0] = float(2 / (r - l));
  m[5] = float(2 / (t - b));
  m[10] = float(-2 / (f - n));
  m[12] = float(-(r + l) / (r - l));
  m[13] = float(-(t + b) / (t - b));
  m[14] = float(-(f + n) / (f - n));
  m[15] = 1;
  glMultMatrixf(m.data());
}
void glFrustum(double l, double r, double b, double t, double n, double f) {
  Matrix m{};
  m[0] = float(2 * n / (r - l));
  m[5] = float(2 * n / (t - b));
  m[8] = float((r + l) / (r - l));
  m[9] = float((t + b) / (t - b));
  m[10] = float(-(f + n) / (f - n));
  m[11] = -1;
  m[14] = float(-2 * f * n / (f - n));
  glMultMatrixf(m.data());
}
void glGetDoublev(GLenum mode, double *values) {
  const auto &m =
      mode == GL_PROJECTION_MATRIX ? context.projection :
      mode == GL_TEXTURE_MATRIX ? context.textureMatrix : context.model;
  std::copy(m.begin(), m.end(), values);
}
void storylandGlGetIntegerv(GLenum name, GLint *values) {
  if (!values) return;
  if (name == GL_CLIENT_ACTIVE_TEXTURE) *values = GLint(GL_TEXTURE0 + context.textureUnit);
  else if (name == GL_MAX_TEXTURE_UNITS) *values = 2;
  else if (name == GL_MATRIX_MODE) *values = GLint(context.matrixMode);
  else if (name == GL_CURRENT_PROGRAM) *values = GLint(context.program);
  else glGetIntegerv(name, values);
}
void storylandGlGetFloatv(GLenum name, GLfloat *values) {
  if (!values) return;
  if (name == GL_MODELVIEW_MATRIX) std::copy(context.model.begin(), context.model.end(), values);
  else if (name == GL_PROJECTION_MATRIX) std::copy(context.projection.begin(), context.projection.end(), values);
  else if (name == GL_TEXTURE_MATRIX) std::copy(context.textureMatrix.begin(), context.textureMatrix.end(), values);
  else if (name == GL_CURRENT_COLOR) std::copy_n(context.current.color, 4, values);
  else glGetFloatv(name, values);
}
void glBegin(GLenum mode) {
  context.primitive = mode;
  context.immediate.clear();
}
void glEnd() {
  draw(context.primitive, context.immediate);
  context.immediate.clear();
}
void glVertex3f(float x, float y, float z) {
  context.current.position[0] = x;
  context.current.position[1] = y;
  context.current.position[2] = z;
  if (context.immediate.size() < 131072)
    context.immediate.push_back(context.current);
}
void glVertex2f(float x, float y) { glVertex3f(x, y, 0); }
void glColor4f(float r, float g, float b, float a) {
  context.current.color[0] = r;
  context.current.color[1] = g;
  context.current.color[2] = b;
  context.current.color[3] = a;
}
void glTexCoord2f(float u, float v) {
  context.current.uv[0] = u;
  context.current.uv[1] = v;
}
void glNormal3f(float x, float y, float z) {
  context.current.normal[0] = x;
  context.current.normal[1] = y;
  context.current.normal[2] = z;
}
void glNormalPointer(GLenum type, GLsizei stride, const void *pointer) {
  context.normal.pointer =
      type == GL_FLOAT ? static_cast<const unsigned char *>(pointer) : nullptr;
  context.normal.size = 3;
  context.normal.stride = stride;
}
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *p) {
  context.position.pointer =
      type == GL_FLOAT ? static_cast<const unsigned char *>(p) : nullptr;
  context.position.size = size;
  context.position.stride = stride;
}
void glColorPointer(GLint size, GLenum type, GLsizei stride, const void *p) {
  context.color.pointer = (type == GL_FLOAT || type == GL_UNSIGNED_BYTE) ? static_cast<const unsigned char *>(p) : nullptr;
  context.color.type = type;
  context.color.size = size;
  context.color.stride = stride;
}
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *p) {
  auto &a = context.uv[context.textureUnit];
  a.pointer =
      type == GL_FLOAT ? static_cast<const unsigned char *>(p) : nullptr;
  a.size = size;
  a.stride = stride;
}
void glEnableClientState(GLenum state) {
  if (state == GL_NORMAL_ARRAY)
    context.normal.enabled = true;
  if (state == GL_VERTEX_ARRAY)
    context.position.enabled = true;
  if (state == GL_COLOR_ARRAY)
    context.color.enabled = true;
  if (state == GL_TEXTURE_COORD_ARRAY)
    context.uv[context.textureUnit].enabled = true;
}
void glDisableClientState(GLenum state) {
  if (state == GL_NORMAL_ARRAY)
    context.normal.enabled = false;
  if (state == GL_VERTEX_ARRAY)
    context.position.enabled = false;
  if (state == GL_COLOR_ARRAY)
    context.color.enabled = false;
  if (state == GL_TEXTURE_COORD_ARRAY)
    context.uv[context.textureUnit].enabled = false;
}
void glClientActiveTexture(GLenum unit) {
  context.textureUnit = unit == GL_TEXTURE1 ? 1 : 0;
}
void storylandGlDrawArrays(GLenum mode, GLint first, GLsizei count) {
  if (first < 0 || count <= 0)
    return;
  std::vector<Vertex> vertices(size_t(count), context.current);
  for (int i = 0; i < count; ++i) {
    auto &v = vertices[size_t(i)];
    copyAttribute(context.position, first + i, v.position, 4);
    copyAttribute(context.color, first + i, v.color, 4);
    copyAttribute(context.uv[0], first + i, v.uv, 2);
    copyAttribute(context.uv[1], first + i, v.uv1, 2);
    copyAttribute(context.normal, first + i, v.normal, 3);
  }
  draw(mode, vertices);
}
void storylandGlDrawElements(GLenum mode, GLsizei count, GLenum type, const void* source) {
  if (count <= 0 || !source || (type != GL_UNSIGNED_INT && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_BYTE)) return;
  std::vector<uint32_t> indices(static_cast<size_t>(count));
  for (size_t index = 0; index < indices.size(); ++index) {
    if (type == GL_UNSIGNED_INT) indices[index] = static_cast<const uint32_t*>(source)[index];
    else if (type == GL_UNSIGNED_SHORT) indices[index] = static_cast<const uint16_t*>(source)[index];
    else indices[index] = static_cast<const uint8_t*>(source)[index];
  }
  const auto bounds = std::minmax_element(indices.begin(), indices.end());
  const uint32_t first = *bounds.first, last = *bounds.second;
  if (last > uint32_t(std::numeric_limits<int>::max())) return;
  std::vector<Vertex> vertices(size_t(last) - first + 1, context.current);
  for (size_t index = 0; index < vertices.size(); ++index) {
    auto& vertex = vertices[index];
    const int sourceIndex = int(first + index);
    copyAttribute(context.position, sourceIndex, vertex.position, 4);
    copyAttribute(context.color, sourceIndex, vertex.color, 4);
    copyAttribute(context.uv[0], sourceIndex, vertex.uv, 2);
    copyAttribute(context.uv[1], sourceIndex, vertex.uv1, 2);
    copyAttribute(context.normal, sourceIndex, vertex.normal, 3);
  }
  for (auto& index : indices) index -= first;
  if (mode == GL_TRIANGLES && context.normal.enabled) {
    std::vector<bool> missing(vertices.size());
    bool needsNormals = false;
    for (size_t index = 0; index < vertices.size(); ++index) {
      const auto* normal = vertices[index].normal;
      const float length = std::hypot(normal[0], normal[1], normal[2]);
      missing[index] = !std::isfinite(length) || length < 1e-8f;
      needsNormals = needsNormals || missing[index];
      if (missing[index]) std::fill(vertices[index].normal, vertices[index].normal + 3, 0.0f);
    }
    if (needsNormals) {
      for (size_t triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
        const auto* a = vertices[indices[triangle]].position;
        const auto* b = vertices[indices[triangle + 1]].position;
        const auto* c = vertices[indices[triangle + 2]].position;
        const float ax = b[0]-a[0], ay = b[1]-a[1], az = b[2]-a[2];
        const float bx = c[0]-a[0], by = c[1]-a[1], bz = c[2]-a[2];
        const float normal[] = {ay*bz-az*by, az*bx-ax*bz, ax*by-ay*bx};
        for (size_t corner = 0; corner < 3; ++corner) {
          const auto index = indices[triangle + corner];
          if (missing[index]) for (size_t axis = 0; axis < 3; ++axis) vertices[index].normal[axis] += normal[axis];
        }
      }
      for (size_t index = 0; index < vertices.size(); ++index) if (missing[index]) {
        auto* normal = vertices[index].normal;
        const float length = std::hypot(normal[0], normal[1], normal[2]);
        if (std::isfinite(length) && length > 1e-8f) for (size_t axis = 0; axis < 3; ++axis) normal[axis] /= length;
        else { normal[0] = normal[1] = 0; normal[2] = 1; }
      }
    }
  }
  draw(mode, vertices, &indices);
}
void glAlphaFunc(GLenum function, float value) {
  context.alphaFunction = function;
  context.alphaLimit = value;
}
void glPointSize(float value) { context.pointSize = value; }
void glPushAttrib(GLbitfield bits) {
  SavedState state;
  state.bits = bits;
  state.texture = context.texture;
  state.alpha = context.alpha;
  state.alphaLimit = context.alphaLimit;
  state.alphaFunction = context.alphaFunction;
  state.pointSize = context.pointSize;
  state.matrix = context.matrixMode;
  state.depth = glIsEnabled(GL_DEPTH_TEST);
  state.blend = glIsEnabled(GL_BLEND);
  state.cull = glIsEnabled(GL_CULL_FACE);
  state.scissor = glIsEnabled(GL_SCISSOR_TEST);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &state.writeDepth);
  glGetBooleanv(GL_COLOR_WRITEMASK, state.writeColor.data());
  glGetIntegerv(GL_DEPTH_FUNC, &state.depthFunction);
  glGetIntegerv(GL_BLEND_SRC_RGB, &state.source);
  glGetIntegerv(GL_BLEND_DST_RGB, &state.destination);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.sourceAlpha);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &state.destinationAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &state.blendRgb);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state.blendAlpha);
  glGetIntegerv(GL_CULL_FACE_MODE, &state.cullFace);
  glGetIntegerv(GL_FRONT_FACE, &state.frontFace);
  glGetIntegerv(GL_VIEWPORT, state.viewport.data());
  glGetIntegerv(GL_SCISSOR_BOX, state.scissorBox.data());
  glGetIntegerv(GL_ACTIVE_TEXTURE, &state.activeTexture);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, state.clearColor.data());
  glGetFloatv(GL_DEPTH_CLEAR_VALUE, &state.clearDepth);
  glGetFloatv(GL_LINE_WIDTH, &state.line);
  if (bits & GL_TEXTURE_BIT) {
    for (unsigned unit = 0; unit < state.textures.size(); ++unit) {
      glActiveTexture(GL_TEXTURE0 + unit);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.textures[unit]);
      glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &state.cubes[unit]);
    }
    glActiveTexture(GLenum(state.activeTexture));
  }
  std::copy(context.current.color, context.current.color + 4,
            state.color.begin());
  if (context.states.size() < 64)
    context.states.push_back(state);
}
void glPopAttrib() {
  if (context.states.empty())
    return;
  const auto state = context.states.back();
  context.states.pop_back();
  if (state.bits & GL_ENABLE_BIT) {
    context.texture = state.texture;
    context.alpha = state.alpha;
    for (auto entry : {std::pair<GLenum, bool>{GL_DEPTH_TEST, state.depth},
                       {GL_BLEND, state.blend}, {GL_CULL_FACE, state.cull},
                       {GL_SCISSOR_TEST, state.scissor}}) {
      if (entry.second) glEnable(entry.first);
      else glDisable(entry.first);
    }
  }
  if (state.bits & GL_TRANSFORM_BIT) context.matrixMode = state.matrix;
  if (state.bits & GL_DEPTH_BUFFER_BIT) {
    glDepthMask(state.writeDepth);
    glDepthFunc(GLenum(state.depthFunction));
    glClearDepthf(state.clearDepth);
  }
  if (state.bits & GL_COLOR_BUFFER_BIT) {
    context.alphaLimit = state.alphaLimit;
    context.alphaFunction = state.alphaFunction;
    glBlendFuncSeparate(GLenum(state.source), GLenum(state.destination),
        GLenum(state.sourceAlpha), GLenum(state.destinationAlpha));
    glBlendEquationSeparate(GLenum(state.blendRgb), GLenum(state.blendAlpha));
    glColorMask(state.writeColor[0], state.writeColor[1], state.writeColor[2], state.writeColor[3]);
    glClearColor(state.clearColor[0], state.clearColor[1], state.clearColor[2], state.clearColor[3]);
  }
  if (state.bits & GL_POLYGON_BIT) {
    glCullFace(GLenum(state.cullFace));
    glFrontFace(GLenum(state.frontFace));
  }
  if (state.bits & GL_VIEWPORT_BIT)
    glViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
  if (state.bits & 0x00080000u)
    glScissor(state.scissorBox[0], state.scissorBox[1], state.scissorBox[2], state.scissorBox[3]);
  if (state.bits & GL_LINE_BIT) glLineWidth(state.line);
  if (state.bits & GL_POINT_BIT) context.pointSize = state.pointSize;
  if (state.bits & GL_CURRENT_BIT)
    std::copy(state.color.begin(), state.color.end(), context.current.color);
  if (state.bits & GL_TEXTURE_BIT) {
    for (unsigned unit = 0; unit < state.textures.size(); ++unit) {
      glActiveTexture(GL_TEXTURE0 + unit);
      glBindTexture(GL_TEXTURE_2D, GLuint(state.textures[unit]));
      glBindTexture(GL_TEXTURE_CUBE_MAP, GLuint(state.cubes[unit]));
    }
    glActiveTexture(GLenum(state.activeTexture));
  }
}
void glPushClientAttrib(GLbitfield bits) {
  if (!(bits & GL_CLIENT_VERTEX_ARRAY_BIT)) return;
  SavedClientState state;
  state.position = context.position;
  state.color = context.color;
  state.normal = context.normal;
  std::copy_n(context.uv, 2, state.uv.begin());
  state.textureUnit = context.textureUnit;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vertexArray);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state.arrayBuffer);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &state.elementBuffer);
  if (context.clientStates.size() < 64) context.clientStates.push_back(state);
}
void glPopClientAttrib() {
  if (context.clientStates.empty()) return;
  const auto state = context.clientStates.back();
  context.clientStates.pop_back();
  context.position = state.position;
  context.color = state.color;
  context.normal = state.normal;
  std::copy(state.uv.begin(), state.uv.end(), context.uv);
  context.textureUnit = state.textureUnit;
  glBindVertexArray(GLuint(state.vertexArray));
  glBindBuffer(GL_ARRAY_BUFFER, GLuint(state.arrayBuffer));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLuint(state.elementBuffer));
}
