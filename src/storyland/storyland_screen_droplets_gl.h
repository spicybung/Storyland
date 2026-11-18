#pragma once

#include "src/storyland/storyland_screen_droplets.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif
#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

class StorylandScreenDropletsGl {
public:
    void reset() {playback_.reset();}

    template<class ActiveTexture,class MultiTexCoord>
    void draw(bool enabled,float gameSeconds,std::uint32_t waterSerial,
        std::uint32_t bloodSerial,float waterStrength,float bloodStrength,
        const StorylandScreenDropletCamera& camera,const void* context,
        ActiveTexture activeTexture,MultiTexCoord multiTexCoord) {
        if(context!=context_) {
            context_=context;sceneTexture_=maskTexture_=0;
            textureWidth_=textureHeight_=0;
            reset();
        }
        if(!enabled) {
            playback_.advance(false,gameSeconds,waterSerial,bloodSerial,waterStrength,
                bloodStrength,camera,1,1);
            return;
        }
        if(!std::isfinite(gameSeconds)||!context||!activeTexture||!multiTexCoord)return;
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT,viewport);
        const int width=viewport[2],height=viewport[3];
        if(width<=0||height<=0||width>8192||height>8192)return;
        playback_.advance(true,gameSeconds,waterSerial,bloodSerial,waterStrength,
            bloodStrength,camera,width,height);
        const auto& simulation=playback_.simulation();
        if(simulation.count()==0)return;

        GLint previousActive=GL_TEXTURE0,previousMatrix=GL_MODELVIEW,textureUnits=0;
        glGetIntegerv(GL_ACTIVE_TEXTURE,&previousActive);
        glGetIntegerv(GL_MATRIX_MODE,&previousMatrix);
        glGetIntegerv(GL_MAX_TEXTURE_UNITS,&textureUnits);
        if(textureUnits<2)return;
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        for(int unit=0;unit<std::min(textureUnits,32);++unit) {
            activeTexture(GL_TEXTURE0+unit);
            glDisable(GL_TEXTURE_1D);glDisable(GL_TEXTURE_2D);
            glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);
            glDisable(GL_TEXTURE_GEN_R);glDisable(GL_TEXTURE_GEN_Q);
#ifdef GL_TEXTURE_3D
            glDisable(GL_TEXTURE_3D);
#endif
#ifdef GL_TEXTURE_CUBE_MAP
            glDisable(GL_TEXTURE_CUBE_MAP);
#endif
#ifdef GL_TEXTURE_RECTANGLE
            glDisable(GL_TEXTURE_RECTANGLE);
#endif
        }
        activeTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        if(!maskTexture_||!glIsTexture(maskTexture_)) {
            const auto pixels=storylandScreenDropletMask();
            glGenTextures(1,&maskTexture_);glBindTexture(GL_TEXTURE_2D,maskTexture_);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
            glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            glPopClientAttrib();
        } else glBindTexture(GL_TEXTURE_2D,maskTexture_);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();

        activeTexture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        if(!sceneTexture_||!glIsTexture(sceneTexture_)) {
            glGenTextures(1,&sceneTexture_);textureWidth_=textureHeight_=0;
        }
        glBindTexture(GL_TEXTURE_2D,sceneTexture_);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        if(width!=textureWidth_||height!=textureHeight_) {
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            textureWidth_=width;textureHeight_=height;
        }
        glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,viewport[0],viewport[1],width,height);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();

        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_FOG);
        glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
        glOrtho(0.0,width,height,0.0,-1.0,1.0);
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
        glBegin(GL_TRIANGLES);
        constexpr int order[6]={0,1,2,2,3,0};
        for(const auto& drop:simulation.droplets())if(drop.active&&drop.color[3]) {
            const auto vertices=storylandScreenDropletQuad(drop,width,height);
            glColor4ub(drop.color[0],drop.color[1],drop.color[2],drop.color[3]);
            for(int index:order) {
                const auto& vertex=vertices[std::size_t(index)];
                multiTexCoord(GL_TEXTURE0,vertex.maskU,vertex.maskV);
                multiTexCoord(GL_TEXTURE1,vertex.sceneU,vertex.sceneV);
                glVertex2f(vertex.x,vertex.y);
            }
        }
        glEnd();
        glMatrixMode(GL_MODELVIEW);glPopMatrix();
        glMatrixMode(GL_PROJECTION);glPopMatrix();
        activeTexture(GL_TEXTURE1);glMatrixMode(GL_TEXTURE);glPopMatrix();
        activeTexture(GL_TEXTURE0);glMatrixMode(GL_TEXTURE);glPopMatrix();
        glPopAttrib();
        activeTexture(GLenum(previousActive));glMatrixMode(GLenum(previousMatrix));
    }

    void release(const void* currentContext) {
        if(currentContext&&currentContext==context_) {
            if(sceneTexture_)glDeleteTextures(1,&sceneTexture_);
            if(maskTexture_)glDeleteTextures(1,&maskTexture_);
        }
        sceneTexture_=maskTexture_=0;textureWidth_=textureHeight_=0;
        context_=nullptr;reset();
    }

    std::size_t count() const {return playback_.simulation().count();}

private:
    StorylandScreenDropletPlayback playback_;
    const void* context_=nullptr;
    GLuint sceneTexture_=0,maskTexture_=0;
    int textureWidth_=0,textureHeight_=0;
};
