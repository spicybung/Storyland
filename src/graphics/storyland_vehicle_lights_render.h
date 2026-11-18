#pragma once
#include "src/storyland/storyland_vehicle_lights.h"
#include "src/graphics/storyland_pickup_render.h"

template<class Texture>
void storylandDrawVehicleLights(const StorylandVehicleLightFrame& lights,
    const StorylandNativeWorld& world,const std::array<float,3>& camera,
    Texture textureFor,float spriteSize=1,float fogginess=0,bool emergencyEmission=false) {
    GLint matrixMode=GL_MODELVIEW;
    glGetIntegerv(GL_MATRIX_MODE,&matrixMode);
    glPushAttrib(GL_TEXTURE_BIT|GL_ENABLE_BIT|GL_CURRENT_BIT);
    glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();glMatrixMode(GL_MODELVIEW);
#ifndef __ANDROID__
    glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_R);glDisable(GL_TEXTURE_GEN_Q);
#endif
    const GLuint beamTexture=lights.beam.visible?textureFor("headlight_single"):0;
    if(beamTexture) {
        glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_TEXTURE_BIT|GL_POLYGON_BIT);
        glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D,beamTexture);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        glColor4f(145.0f/255,145.0f/255,145.0f/255,1);
#ifndef __ANDROID__
        glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-1,-1);
#endif
        glBegin(GL_TRIANGLES);
        storylandVisitPickupGroundGlow(world,lights.beam,camera,[](const StorylandPickupGlowVertex& vertex) {
            glTexCoord2f(vertex.v,vertex.u);glVertex3fv(vertex.position.data());
        });
        glEnd();glPopAttrib();
    }
    if(lights.sprites.coronaCount) {
        auto sprites=lights.sprites;
        for(size_t index=0;index<sprites.coronaCount;++index)sprites.coronas[index].radius*=spriteSize;
        storylandDrawVcsPickupVisuals(std::vector<StorylandVcsPickupVisual>{sprites},
            world,camera,textureFor,fogginess);
    }
    if(lights.emergency.coronaCount) {
        auto sprites=lights.emergency;
        for(size_t index=0;index<sprites.coronaCount;++index)sprites.coronas[index].radius*=spriteSize;
        storylandDrawVcsPickupVisuals(std::vector<StorylandVcsPickupVisual>{sprites},
            world,camera,textureFor,fogginess);
        if(emergencyEmission) {
            for(size_t index=0;index<sprites.coronaCount;++index) {
                auto& corona=sprites.coronas[index];
                corona.texture=1;corona.radius*=2.6f;
                for(auto& channel:corona.color)channel=uint8_t(float(channel)*.22f);
            }
            storylandDrawVcsPickupVisuals(std::vector<StorylandVcsPickupVisual>{sprites},
                world,camera,textureFor,fogginess);
        }
    }
    glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(GLenum(matrixMode));glPopAttrib();
}
