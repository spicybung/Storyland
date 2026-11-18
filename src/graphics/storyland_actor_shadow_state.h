#pragma once

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif

// Called between glPushAttrib and glPopAttrib with a current GL context.
struct StorylandActorShadowTextureState {
    GLint activeTexture=GL_TEXTURE0;
    GLint matrixMode=GL_MODELVIEW;
    bool restoreActive=false;
};

template<class SelectTexture>
StorylandActorShadowTextureState storylandBeginActorShadowTextureState(bool multitexture, const SelectTexture& selectTexture) {
    StorylandActorShadowTextureState saved;
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
        saved.restoreActive=true;
    }
    glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
#ifndef __ANDROID__
    glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_R);glDisable(GL_TEXTURE_GEN_Q);
#endif
    glDisable(GL_FOG);
    return saved;
}

template<class SelectTexture>
void storylandEndActorShadowTextureState(const StorylandActorShadowTextureState& saved,const SelectTexture& selectTexture) {
    if(saved.restoreActive)selectTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);glPopMatrix();
    glMatrixMode(GLenum(saved.matrixMode));
    glPopAttrib();
    if(saved.restoreActive)selectTexture(GLenum(saved.activeTexture));
}
