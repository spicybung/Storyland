#define NOMINMAX
#include "storyland_opengl_renderer.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <cmath>

void StorylandOpenGLRenderer::BeginFrame(int width, int height) const {
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);
}

void StorylandOpenGLRenderer::ResetSurfaceState() const {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);
}

void StorylandOpenGLRenderer::DrawGrid(float extent, float step) const {
    if (!(extent > 0.0f) || !(step > 0.0f) ||
        !std::isfinite(extent) || !std::isfinite(step)) return;

    const int lines = (std::min)(256, (std::max)(1, int(std::ceil(extent / step))));
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glColor3f(0.20f, 0.21f, 0.23f);
    for (int i = -lines; i <= lines; ++i) {
        const float d = float(i) * step;
        glVertex3f(-extent, d, 0.0f); glVertex3f(extent, d, 0.0f);
        glVertex3f(d, -extent, 0.0f); glVertex3f(d, extent, 0.0f);
    }
    glColor3f(0.95f, 0.22f, 0.18f);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(extent, 0.0f, 0.0f);
    glColor3f(0.20f, 0.85f, 0.25f);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, extent, 0.0f);
    glColor3f(0.25f, 0.48f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, extent);
    glEnd();
}

void StorylandOpenGLRenderer::DrawBounds(
    float minX, float minY, float minZ,
    float maxX, float maxY, float maxZ) const {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
    glColor3f(0.55f, 0.58f, 0.62f);

    const float p[8][3] = {
        {minX,minY,minZ},{maxX,minY,minZ},{maxX,maxY,minZ},{minX,maxY,minZ},
        {minX,minY,maxZ},{maxX,minY,maxZ},{maxX,maxY,maxZ},{minX,maxY,maxZ}
    };
    const int e[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    glBegin(GL_LINES);
    for (const auto& edge : e) {
        glVertex3fv(p[edge[0]]);
        glVertex3fv(p[edge[1]]);
    }
    glEnd();
}

void StorylandOpenGLRenderer::DrawSimpleSurface(
    const std::vector<StorylandModelPoint>& points,
    const std::vector<StorylandModelTriangle>& triangles,
    const std::vector<StorylandModelTexcoord>& texcoords,
    StorylandGlSurfaceMode mode,
    unsigned int textureId,
    bool flipV) const {
    const bool textured =
        mode == StorylandGlSurfaceMode::Textured &&
        textureId != 0u &&
        texcoords.size() == points.size();

    if (textured) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    if (mode == StorylandGlSurfaceMode::Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(0.86f, 0.88f, 0.82f);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColor3f(mode == StorylandGlSurfaceMode::Solid ? 0.76f : 1.0f,
                  mode == StorylandGlSurfaceMode::Solid ? 0.78f : 1.0f,
                  mode == StorylandGlSurfaceMode::Solid ? 0.74f : 1.0f);
    }

    glBegin(GL_TRIANGLES);
    for (const StorylandModelTriangle& tri : triangles) {
        const uint32_t indices[3] = {tri.a, tri.b, tri.c};
        if (indices[0] >= points.size() ||
            indices[1] >= points.size() ||
            indices[2] >= points.size()) continue;

        const StorylandModelPoint& a = points[indices[0]];
        const StorylandModelPoint& b = points[indices[1]];
        const StorylandModelPoint& c = points[indices[2]];
        const float abx = b.x-a.x, aby=b.y-a.y, abz=b.z-a.z;
        const float acx = c.x-a.x, acy=c.y-a.y, acz=c.z-a.z;
        float nx = aby*acz - abz*acy;
        float ny = abz*acx - abx*acz;
        float nz = abx*acy - aby*acx;
        const float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (len > 0.0000001f) { nx/=len; ny/=len; nz/=len; }
        glNormal3f(nx,ny,nz);

        for (uint32_t index : indices) {
            if (textured) {
                float u = texcoords[index].u;
                float v = texcoords[index].v;
                if (flipV) v = 1.0f - v;
                glTexCoord2f(u, v);
            }
            const StorylandModelPoint& point = points[index];
            glVertex3f(point.x, point.y, point.z);
        }
    }
    glEnd();

    ResetSurfaceState();
}
