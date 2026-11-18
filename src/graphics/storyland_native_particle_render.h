#pragma once
#include "src/graphics/storyland_particle_render.h"

inline void storylandDrawNativeParticle(const StorylandRuntimeParticle& particle,GLuint texture,
    const float* view,const float* projection,float nativeFieldOfView) {
    StorylandNativeParticleQuad quad;
    if(!texture||!storylandNativeParticleQuad(particle,view,projection,nativeFieldOfView,quad))return;
    const auto blend=storylandNativeParticleBlend(particle);
    glBlendFunc(blend==StorylandNativeParticleBlend::Additive?GL_ONE:GL_SRC_ALPHA,
        blend==StorylandNativeParticleBlend::Alpha?GL_ONE_MINUS_SRC_ALPHA:GL_ONE);
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
    const auto color=storylandNativeParticleColor(particle);
    glColor4ub(color[0],color[1],color[2],color[3]);
    glBegin(GL_QUADS);
    for(size_t index=0;index<quad.vertices.size();++index) {
        glTexCoord2fv(quad.uv[index].data());
        const auto& point=quad.vertices[index];glVertex3f(point.x,point.y,point.z);
    }
    glEnd();
}
