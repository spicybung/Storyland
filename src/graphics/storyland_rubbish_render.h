#pragma once

#include "src/storyland/storyland_play_rubbish.h"
#include "src/storyland/storyland_play_rubbish_mesh.h"
#include <algorithm>
#include <cmath>

// Called with a current compatibility-profile rendering context.
template<class TextureFor>
void storylandDrawRubbish(const StorylandPlayRubbish& rubbish,const TextureFor& textureFor,
    bool fourthTextureEnabled=true) {
    if(!rubbish.enabled||rubbish.sheets().empty())return;
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT|GL_TEXTURE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    for(const auto& sheet:rubbish.sheets()) {
        if(sheet.kind==StorylandRubbishKind::NewspaperB&&!fourthTextureEnabled)continue;
        const float alpha=std::clamp(sheet.opacity,0.0f,1.0f);
        if(alpha<=0.0f)continue;
        const float c=std::cos(sheet.yaw),s=std::sin(sheet.yaw);
        const float rollCos=std::cos(sheet.roll),rollSin=std::sin(sheet.roll);
        const auto vertex=[&](float x,float y,float z) {
            const float rolledY=y*rollCos-z*rollSin;
            glVertex3f(sheet.position.x+x*c-rolledY*s,
                       sheet.position.y+x*s+rolledY*c,
                       sheet.position.z+y*rollSin+z*rollCos);
        };
        if(sheet.kind==StorylandRubbishKind::Branch) {
            glDisable(GL_TEXTURE_2D);
            glBegin(GL_TRIANGLES);
            for(const auto& point:storylandRubbishBranchMesh(sheet)) {
                glColor4f(point.r,point.g,point.b,alpha);
                vertex(point.x,point.y,point.z);
            }
            glEnd();
            continue;
        }
        const GLuint texture=textureFor(storylandRubbishTextureName(sheet.kind));
        if(!texture)continue;
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D,texture);
        glColor4f(1.0f,1.0f,1.0f,alpha);
        const float x=sheet.width*.5f,y=sheet.height*.5f;
        glBegin(GL_QUADS);
        glTexCoord2f(0,0);vertex(-x,-y,0);
        glTexCoord2f(1,0);vertex( x,-y,0);
        glTexCoord2f(1,1);vertex( x, y,0);
        glTexCoord2f(0,1);vertex(-x, y,0);
        glEnd();
    }
    glPopAttrib();
}
