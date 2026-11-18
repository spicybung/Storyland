#pragma once

#include "src/storyland/storyland_weapon_visuals.h"

inline void storylandDrawWeaponTraces(const std::vector<StorylandWeaponTrace>& traces,bool emission=false) {
    if(traces.empty())return;
    float view[16]{};
    glGetFloatv(GL_MODELVIEW_MATRIX,view);
    glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);glEnable(GL_BLEND);glDepthMask(GL_FALSE);
    for(const auto& trace:traces) {
        if(trace.vcs) {
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_TRIANGLES);
            storylandVcsBulletTraceTriangles(trace,view,[](const StorylandBulletTraceVertex& vertex) {
                glColor4ub(255,152,34,vertex.alpha);
                glVertex3f(vertex.position.x,vertex.position.y,vertex.position.z);
            });
            glEnd();
            if(emission) {
                auto glow=trace;glow.radiusScale*=3.0f;
                glBlendFunc(GL_SRC_ALPHA,GL_ONE);
                glBegin(GL_TRIANGLES);
                storylandVcsBulletTraceTriangles(glow,view,[](const StorylandBulletTraceVertex& vertex) {
                    glColor4ub(255,143,40,uint8_t(float(vertex.alpha)*.20f));
                    glVertex3f(vertex.position.x,vertex.position.y,vertex.position.z);
                });
                glEnd();
            }
        } else {
            const auto line=storylandWeaponTraceSegment(trace);
            if(!line.visible)continue;
            glBlendFunc(GL_SRC_ALPHA,GL_ONE);
            glLineWidth(line.width);
            glBegin(GL_LINES);
            glColor4f(.98f,.85f,.58f,0.0f);
            glVertex3f(line.tail.x,line.tail.y,line.tail.z);
            glColor4f(1.0f,.95f,.72f,line.alpha);
            glVertex3f(line.head.x,line.head.y,line.head.z);
            glEnd();
            if(emission) {
                glLineWidth(line.width*3.0f);
                glBegin(GL_LINES);
                glColor4f(1.0f,.65f,.20f,0);
                glVertex3f(line.tail.x,line.tail.y,line.tail.z);
                glColor4f(1.0f,.65f,.20f,line.alpha*.23f);
                glVertex3f(line.head.x,line.head.y,line.head.z);
                glEnd();
            }
        }
    }
    glPopAttrib();
}
