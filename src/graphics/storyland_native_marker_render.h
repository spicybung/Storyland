#pragma once

#include "src/leeds/leeds_model.h"
#include <array>
#include <cstdint>
#include <vector>

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif

struct StorylandNativeMarkerRenderState {
    GLint activeTexture=GL_TEXTURE0;
    GLint matrixMode=GL_MODELVIEW;
    bool multitexture=false;
};

template<class SelectTexture>
StorylandNativeMarkerRenderState storylandBeginNativeMarkers(bool multitexture,
    const SelectTexture& selectTexture) {
    StorylandNativeMarkerRenderState saved;
    saved.multitexture=multitexture;
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT|GL_TEXTURE_BIT);
    glGetIntegerv(GL_MATRIX_MODE,&saved.matrixMode);
    if(multitexture) {
        glGetIntegerv(GL_ACTIVE_TEXTURE,&saved.activeTexture);
        GLint units=1;
        glGetIntegerv(0x84E2,&units);
        for(GLint unit=1;unit<units;++unit) {
            selectTexture(GL_TEXTURE0+unit);
            glDisable(GL_TEXTURE_2D);
        }
        selectTexture(GL_TEXTURE0);
    }
    glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
#ifndef __ANDROID__
    glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_R);glDisable(GL_TEXTURE_GEN_Q);
#endif
    glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);glDisable(GL_FOG);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    return saved;
}

template<class SelectTexture>
void storylandEndNativeMarkers(const StorylandNativeMarkerRenderState& saved,
    const SelectTexture& selectTexture) {
    if(saved.multitexture)selectTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);glPopMatrix();
    glMatrixMode(GLenum(saved.matrixMode));
    glPopAttrib();
    if(saved.multitexture)selectTexture(GLenum(saved.activeTexture));
}

template<class VisitVertices,class TextureForName>
void storylandDrawNativeMarker(const StorylandModelFile& model,uint8_t type,
    const VisitVertices& visitVertices,const TextureForName& textureForName) {
    const auto& names=model.previewMaterialTextureNames();
    std::vector<GLuint> textures;
    textures.reserve(names.size());
    for(const auto& name:names)
        textures.push_back(name.empty()?0:textureForName(name.c_str()));

    glDepthMask(type==1u||type==2u||type==3u?GL_TRUE:GL_FALSE);
    size_t activeMaterial=SIZE_MAX;
    GLuint activeTexture=0;
    GLint previousWrapS=GL_REPEAT,previousWrapT=GL_REPEAT;
    bool drawing=false;
    const auto finishMaterial=[&] {
        if(drawing)glEnd();
        if(activeTexture) {
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,previousWrapS);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,previousWrapT);
        }
        drawing=false;
    };
    visitVertices([&](const std::array<float,3>& point,const std::array<float,4>& color,
        const std::array<float,2>& uv,uint32_t material) {
        if(!drawing||activeMaterial!=material) {
            finishMaterial();
            activeMaterial=material;
            activeTexture=material<textures.size()?textures[material]:0;
            if(activeTexture) {
                glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,activeTexture);
                glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&previousWrapS);
                glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,&previousWrapT);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
            } else glDisable(GL_TEXTURE_2D);
            glBegin(GL_TRIANGLES);
            drawing=true;
        }
        glColor4fv(color.data());glTexCoord2fv(uv.data());glVertex3fv(point.data());
    });
    finishMaterial();
}
