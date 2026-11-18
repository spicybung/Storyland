#pragma once
#include "src/storyland/storyland_pickup_ground_glow.h"
#include "src/storyland/storyland_script_corona.h"
#include "src/graphics/storyland_particle_render.h"

template<class Texture>
void storylandDrawVcsPickupVisuals(const std::vector<StorylandVcsPickupVisual>& visuals,
    const StorylandNativeWorld& world,const std::array<float,3>& camera,Texture textureFor,
    float fogginess=0) {
    if(visuals.empty())return;
    GLfloat view[16],projection[16];
    glGetFloatv(GL_MODELVIEW_MATRIX,view);
    glGetFloatv(GL_PROJECTION_MATRIX,projection);
    float right[3],up[3];
    if(!storylandParticleBillboardAxes(view,right,up)||projection[5]==0)return;
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT|GL_TEXTURE_BIT);
    glDisable(GL_LIGHTING);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);glDisable(GL_FOG);
    glEnable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    const GLuint shadow=textureFor("shad_exp");
    if(shadow) {
#ifndef __ANDROID__
        // Ground glows are projected onto collision triangles and can share
        // the ground's quantized depth. Bias only this surface overlay.
        glPushAttrib(GL_ENABLE_BIT|GL_POLYGON_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0.0f,-2.0f);
#endif
        glBindTexture(GL_TEXTURE_2D,shadow);
        glBlendFunc(GL_ONE,GL_ONE);
        for(const auto& visual:visuals) {
            const auto& glow=visual.groundGlow;
            if(!glow.visible)continue;
            glColor4f(float(glow.color[0])/255.0f,float(glow.color[1])/255.0f,
                float(glow.color[2])/255.0f,1);
            glBegin(GL_TRIANGLES);
            storylandVisitPickupGroundGlow(world,glow,camera,[](const StorylandPickupGlowVertex& vertex) {
                glTexCoord2f(vertex.u,vertex.v);
                glVertex3fv(vertex.position.data());
            });
            glEnd();
        }
#ifndef __ANDROID__
        glPopAttrib();
#endif
    }
    glBlendFunc(GL_ONE,GL_ONE);
    for(const auto& visual:visuals)for(size_t index=0;index<visual.coronaCount;++index) {
        const auto& corona=visual.coronas[index];
        if(!(corona.color[0]||corona.color[1]||corona.color[2])||
            !storylandCoronaInRegistrationRange(corona.position,view,corona.range))continue;
        const GLuint texture=textureFor(storylandCoronaTextureName(corona.texture));
        if(!texture)continue;
        StorylandCoronaProjection projected;
        if(!storylandProjectCorona(corona.position,view,projection,projected,
            corona.range,corona.pullAway))continue;
        const float fog=storylandCoronaFogScale(projected.depth,fogginess);
        const auto tint=storylandCoronaFogColor(corona.color,fog);
        const auto alpha=storylandCoronaFadedAlpha(corona.alpha,projected.depth,corona.range);
        const auto color=storylandCoronaSpriteColor(tint,alpha,projected.biasedDepth);
        const float radius=std::abs(corona.radius)*fog*projected.depthScale*
            (2.0f/std::abs(projection[5]));
        glBindTexture(GL_TEXTURE_2D,texture);
        glColor4f(color[0],color[1],color[2],1);
        glBegin(GL_QUADS);
        for(const auto& corner:std::array<std::array<float,2>,4>{{{-1,-1},{1,-1},{1,1},{-1,1}}}) {
            glTexCoord2f((corner[0]+1)*.5f,(1-corner[1])*.5f);
            std::array<float,3> point{};
            for(size_t axis=0;axis<3;++axis)point[axis]=projected.center[axis]+
                right[axis]*corner[0]*radius+up[axis]*corner[1]*radius;
            glVertex3fv(point.data());
        }
        glEnd();
    }
    glPopAttrib();
}
