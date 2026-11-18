#pragma once

#include "src/leeds/leeds_archive.h"
#include <array>

template<class Texture, class UvOffset>
size_t storylandDrawVehicleEnvironmentMesh(const StorylandWorldMesh& mesh,
    const StorylandWorldPlacement& placement, const Texture& textureForMaterial,
    const UvOffset& uvOffsetForMaterial) {
    struct Vertex { float u,v,r,g,b,a,x,y,z; };
    std::array<Vertex,1536> vertices{};
    size_t count=0, triangles=0;
    const bool native=placement.nativeInstanceIndex!=UINT32_MAX;
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3,GL_FLOAT,sizeof(Vertex),&vertices.front().x);
    glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),&vertices.front().u);
    glColorPointer(4,GL_FLOAT,sizeof(Vertex),&vertices.front().r);
    const auto flush=[&]() {
        if(count)glDrawArrays(GL_TRIANGLES,0,GLsizei(count));
        count=0;
    };
    uint32_t previous=UINT32_MAX;
    bool bound=false, textured=false;
    std::array<float, 2> uvOffset{};
    const bool translucent=native?mesh.nativeAlphaBlend:placement.passName=="TRANSPARENT";
    if(translucent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    } else glDisable(GL_BLEND);
    const bool depthWrite=placement.passName!="NOZWRITE"&&(!translucent||mesh.nativeAlphaTest);
    glDepthMask(depthWrite?GL_TRUE:GL_FALSE);
    for(const auto& triangle:mesh.triangles) {
        if(triangle.a>=mesh.vertices.size()||triangle.b>=mesh.vertices.size()||
            triangle.c>=mesh.vertices.size())continue;
        if(!bound||previous!=triangle.textureId) {
            flush();
            previous=triangle.textureId;
            bound=true;
            const GLuint texture=textureForMaterial(previous);
            textured=texture!=0;
            uvOffset = textured ? uvOffsetForMaterial(previous) : std::array<float, 2>{};
            if(textured) {glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);}
            else glDisable(GL_TEXTURE_2D);
            if(native?mesh.nativeAlphaTest:textured) {
                glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.08f);
            } else glDisable(GL_ALPHA_TEST);
        }
        if(count+3>vertices.size())flush();
        const float scale=native||textured?1.0f/128.0f:1.0f/255.0f;
        for(uint32_t index:{triangle.a,triangle.b,triangle.c}) {
            const auto& point=mesh.vertices[index];
            const float red=point.hasPrelight?std::min(1.0f,point.red*scale):1.0f;
            const float green=point.hasPrelight?std::min(1.0f,point.green*scale):1.0f;
            const float blue=point.hasPrelight?std::min(1.0f,point.blue*scale):1.0f;
            const float alpha=native&&point.hasPrelight?point.alpha/255.0f:1.0f;
            vertices[count++]={point.u+uvOffset[0],point.v+uvOffset[1],red,green,blue,alpha,
                point.x,point.y,point.z};
        }
        ++triangles;
    }
    flush();
    return triangles;
}

template<class Texture>
size_t storylandDrawVehicleEnvironmentMesh(const StorylandWorldMesh& mesh,
    const StorylandWorldPlacement& placement, const Texture& textureForMaterial) {
    return storylandDrawVehicleEnvironmentMesh(mesh, placement, textureForMaterial,
        [](uint32_t) { return std::array<float, 2>{}; });
}
