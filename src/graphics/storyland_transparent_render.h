#pragma once

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif

struct StorylandTransparentRenderState {
    GLint activeTexture=GL_TEXTURE0,matrixMode=GL_MODELVIEW;
    bool multitexture=false;
};

template<class SelectTexture>
StorylandTransparentRenderState storylandBeginTransparentPass(bool multitexture,
    const SelectTexture& selectTexture) {
    StorylandTransparentRenderState saved;
    saved.multitexture=multitexture;
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT|
        GL_TEXTURE_BIT|GL_LINE_BIT|GL_POLYGON_BIT);
    glGetIntegerv(GL_MATRIX_MODE,&saved.matrixMode);
    if(multitexture) {
        glGetIntegerv(GL_ACTIVE_TEXTURE,&saved.activeTexture);
        GLint units=1;glGetIntegerv(0x84E2,&units);
        for(GLint unit=1;unit<units;++unit) {
            selectTexture(GL_TEXTURE0+unit);glDisable(GL_TEXTURE_2D);
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
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    return saved;
}

template<class SelectTexture>
void storylandEndTransparentPass(const StorylandTransparentRenderState& saved,
    const SelectTexture& selectTexture) {
    if(saved.multitexture)selectTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);glPopMatrix();
    glMatrixMode(GLenum(saved.matrixMode));
    glPopAttrib();
    if(saved.multitexture)selectTexture(GLenum(saved.activeTexture));
}
