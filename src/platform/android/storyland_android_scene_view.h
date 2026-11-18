#pragma once

#include "src/leeds/leeds_archive.h"
#include "src/leeds/leeds_model.h"
#include "imgui.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_set>
#include <string>
#include <tuple>
#include <cstring>
#include <unordered_map>
#include <vector>

class StorylandAndroidSceneView {
public:
    StorylandAndroidSceneView() = default;
    StorylandAndroidSceneView(const StorylandAndroidSceneView&) = delete;
    StorylandAndroidSceneView& operator=(const StorylandAndroidSceneView&) = delete;

    void release() {
        for (auto& entry : textures) if (entry.second) glDeleteTextures(1, &entry.second);
        textures.clear();
        if (vertexBuffer) glDeleteBuffers(1, &vertexBuffer);
        if (vertexArray) glDeleteVertexArrays(1, &vertexArray);
        if (program) glDeleteProgram(program);
        if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
        if (colourBuffer) glDeleteTextures(1, &colourBuffer);
        if (depthBuffer) glDeleteRenderbuffers(1, &depthBuffer);
        vertexBuffer = vertexArray = program = framebuffer = colourBuffer = depthBuffer = 0;
        frameWidth = frameHeight = 0;
        ranges.clear();
        triangleCount = 0;
    }

    void clearGeometry() {
        for (auto& entry : textures) if (entry.second) glDeleteTextures(1,&entry.second);
        textures.clear();
        if (vertexBuffer) { glDeleteBuffers(1, &vertexBuffer); vertexBuffer = 0; }
        if (vertexArray) { glDeleteVertexArrays(1, &vertexArray); vertexArray = 0; }
        ranges.clear();
        triangleCount = 0;
        ready = false;
        yaw = .65f;
        pitch = .45f;
        distance = 2.6f;
    }

    bool available() const { return ready && triangleCount != 0; }
    size_t triangles() const { return triangleCount; }
    bool truncated() const { return limited; }
    const std::string& error() const { return errorText; }

    void buildModel(const StorylandModelFile& model) {
        clearGeometry();
        errorText.clear();
        limited = false;
        const auto& points = model.previewPoints();
        const auto& triangles = model.previewTriangles();
        std::map<uint32_t, std::vector<Vertex>> groups;
        for (const auto& triangle : triangles) {
            if (triangleCount >= limitTriangles) { limited = true; break; }
            if (triangle.a >= points.size() || triangle.b >= points.size() || triangle.c >= points.size()) continue;
            auto& vertices = groups[0xFFFFFFFFu];
            const auto& a = points[triangle.a];
            const auto& b = points[triangle.b];
            const auto& c = points[triangle.c];
            appendTriangle(vertices, {a.x,a.y,a.z}, {b.x,b.y,b.z}, {c.x,c.y,c.z},
                {1,1,1}, {1,1,1}, {1,1,1}, {0,0}, {0,0}, {0,0});
            ++triangleCount;
        }
        upload(groups);
    }

    void buildArchive(const StorylandArchiveBrowser& archive, int resourceId) {
        clearGeometry();
        errorText.clear();
        limited = false;
        std::map<uint32_t, std::vector<Vertex>> groups;
        const auto& meshes = archive.worldMeshes();
        const auto& placements = archive.placements();
        if (resourceId >= 0) {
            for (const auto& mesh : meshes) {
                if (mesh.resourceIndex != uint32_t(resourceId)) continue;
                appendMesh(groups, mesh, nullptr);
                if (triangleCount >= limitTriangles) break;
            }
        } else {
            std::unordered_map<uint64_t, const StorylandWorldMesh*> lookup;
            lookup.reserve(meshes.size());
            for (const auto& mesh : meshes)
                lookup.emplace((uint64_t(mesh.sectorIndex)<<32)|mesh.resourceIndex, &mesh);
            const size_t stride = std::max<size_t>(1, placements.size()/350);
            for (size_t index = 0; index < placements.size(); index += stride) {
                const auto& placement = placements[index];
                auto it = lookup.find((uint64_t(placement.sectorIndex)<<32)|placement.resourceIndex);
                if (it == lookup.end()) continue;
                appendMesh(groups, *it->second, placement.matrix);
                if (triangleCount >= limitTriangles) break;
            }
            if (stride > 1) limited = true;
            if (triangleCount == 0) for (const auto& mesh : meshes) {
                appendMesh(groups,mesh,nullptr);
                if (triangleCount >= limitTriangles) break;
            }
        }
        upload(groups);
        std::unordered_set<uint32_t> usedMaterials;
        for (const Range& range:ranges) usedMaterials.insert(range.material);
        GLint textureLimit = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,&textureLimit);
        size_t uploadedTextureBytes = 0;
        constexpr size_t textureBudget = 64u * 1024u * 1024u;
        for (const auto& texture : archive.directTextures()) {
            if (texture.rgba.empty() || texture.width <= 0 || texture.height <= 0) continue;
            if (texture.width > textureLimit || texture.height > textureLimit) continue;
            if (texture.width > 2048 || texture.height > 2048) continue;
            const size_t needed = size_t(texture.width)*size_t(texture.height)*4u;
            if (needed > textureBudget-uploadedTextureBytes) continue;
            if (texture.materialId >= 0 && usedMaterials.count(uint32_t(texture.materialId)) &&
                textures.count(uint32_t(texture.materialId)) == 0) {
                if (textures.size() >= 48) break;
                GLuint gpu = 0;
                glGenTextures(1, &gpu);
                if (!gpu) continue;
                glBindTexture(GL_TEXTURE_2D, gpu);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texture.width,texture.height,0,GL_RGBA,GL_UNSIGNED_BYTE,texture.rgba.data());
                textures.emplace(uint32_t(texture.materialId),gpu);
                uploadedTextureBytes += needed;
            }
        }
        glBindTexture(GL_TEXTURE_2D,0);
    }

    void draw(float unit) {
        if (!available()) {
            ImGui::TextWrapped("No decoded triangles are available for this selection.");
            if (!errorText.empty()) ImGui::TextWrapped("%s", errorText.c_str());
            return;
        }
        if (ImGui::SmallButton("Reset view")) { yaw=.65f; pitch=.45f; distance=2.6f; }
        ImGui::SameLine();
        if (ImGui::SmallButton("-")) distance = std::min(20.f, distance*1.25f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+")) distance = std::max(1.1f, distance*.8f);
        ImGui::SameLine();
        ImGui::TextDisabled("%zu triangles%s", triangleCount, limited?" (sampled)":"");
        const auto availableSize = ImGui::GetContentRegionAvail();
        const ImVec2 size {std::max(1.f,availableSize.x),std::max(60*unit,availableSize.y)};
        if (!ensureFrame(int(size.x),int(size.y)) || !ensureProgram()) {
            ImGui::TextWrapped("OpenGL ES 3 preview: %s",errorText.c_str());
            return;
        }
        ImGui::InvisibleButton("3D scene",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            yaw += ImGui::GetIO().MouseDelta.x*.008f;
            pitch = std::clamp(pitch+ImGui::GetIO().MouseDelta.y*.008f,-1.5f,1.5f);
        }
        if (ImGui::IsItemHovered())
            distance = std::clamp(distance-ImGui::GetIO().MouseWheel*.13f,1.1f,20.f);
        render();
        ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)colourBuffer,
            ImGui::GetItemRectMin(),ImGui::GetItemRectMax(),{0,1},{1,0});
    }

private:
    struct Vertex { float pos[3], tint[3], normal[3], uv[2]; };
    struct Range { GLsizei first = 0, count = 0; uint32_t material = 0xFFFFFFFFu; };
    static constexpr size_t limitTriangles = 175000;
    GLuint vertexBuffer = 0, vertexArray = 0, program = 0;
    GLuint framebuffer = 0, colourBuffer = 0, depthBuffer = 0;
    int frameWidth = 0, frameHeight = 0;
    std::vector<Range> ranges;
    std::map<uint32_t,GLuint> textures;
    size_t triangleCount = 0;
    bool limited = false, ready = false;
    float yaw = .65f, pitch = .45f, distance = 2.6f;
    std::array<float,3> minimum{0,0,0}, maximum{0,0,0};
    std::string errorText;

    static std::array<float,3> transform(const std::array<float,3>& point,const float* matrix) {
        if (!matrix) return point;
        return {point[0]*matrix[0]+point[1]*matrix[4]+point[2]*matrix[8]+matrix[12],
            point[0]*matrix[1]+point[1]*matrix[5]+point[2]*matrix[9]+matrix[13],
            point[0]*matrix[2]+point[1]*matrix[6]+point[2]*matrix[10]+matrix[14]};
    }
    static void appendTriangle(std::vector<Vertex>& output,
        const std::array<float,3>& a,const std::array<float,3>& b,const std::array<float,3>& c,
        const std::array<float,3>& ca,const std::array<float,3>& cb,const std::array<float,3>& cc,
        const std::array<float,2>& ua,const std::array<float,2>& ub,const std::array<float,2>& uc) {
        const std::array<float,3> ab{b[0]-a[0],b[1]-a[1],b[2]-a[2]};
        const std::array<float,3> ac{c[0]-a[0],c[1]-a[1],c[2]-a[2]};
        std::array<float,3> normal{ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]};
        const float magnitude=std::max(.00001f,std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]));
        for (float& component:normal) component /= magnitude;
        for (const auto& item:std::array<std::tuple<std::array<float,3>,std::array<float,3>,std::array<float,2>>,3>{
            std::make_tuple(a,ca,ua),std::make_tuple(b,cb,ub),std::make_tuple(c,cc,uc)}) {
            Vertex vertex{};
            const auto& point=std::get<0>(item); const auto& tint=std::get<1>(item); const auto& uv=std::get<2>(item);
            for (int component=0;component<3;++component) {
                vertex.pos[component]=point[component];vertex.tint[component]=tint[component];vertex.normal[component]=normal[component];
            }
            vertex.uv[0]=uv[0];vertex.uv[1]=uv[1];output.push_back(vertex);
        }
    }
    void appendMesh(std::map<uint32_t,std::vector<Vertex>>& groups,const StorylandWorldMesh& mesh,const float* matrix) {
        const auto& points = mesh.vertices;
        for (const auto& triangle:mesh.triangles) {
            if (triangleCount >= limitTriangles) { limited = true; return; }
            if (triangle.a >= points.size() || triangle.b >= points.size() || triangle.c >= points.size()) continue;
            const auto& a=points[triangle.a];const auto& b=points[triangle.b];const auto& c=points[triangle.c];
            const auto colour=[](const auto& vertex) -> std::array<float,3> {
                if (!vertex.hasPrelight) return {1,1,1};
                return {vertex.red/255.f,vertex.green/255.f,vertex.blue/255.f};
            };
            appendTriangle(groups[triangle.textureId],
                transform({a.x,a.y,a.z},matrix),transform({b.x,b.y,b.z},matrix),transform({c.x,c.y,c.z},matrix),
                colour(a),colour(b),colour(c),{a.u,a.v},{b.u,b.v},{c.u,c.v});
            ++triangleCount;
        }
    }
    void upload(const std::map<uint32_t,std::vector<Vertex>>& groups) {
        std::vector<Vertex> vertices;
        vertices.reserve(triangleCount*3);
        bool boundsReady=false;
        for (const auto& entry:groups) {
            Range range;
            range.first=GLsizei(vertices.size());
            range.count=GLsizei(entry.second.size());
            range.material=entry.first;
            ranges.push_back(range);
            for (const auto& vertex:entry.second) {
                if (!boundsReady) {
                    for (int c=0;c<3;++c) minimum[c]=maximum[c]=vertex.pos[c];
                    boundsReady=true;
                } else for (int c=0;c<3;++c) {
                    minimum[c]=std::min(minimum[c],vertex.pos[c]);
                    maximum[c]=std::max(maximum[c],vertex.pos[c]);
                }
                vertices.push_back(vertex);
            }
        }
        if (vertices.empty()) return;
        glGenVertexArrays(1,&vertexArray);
        glBindVertexArray(vertexArray);
        glGenBuffers(1,&vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER,vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(Vertex)),vertices.data(),GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(const void*)offsetof(Vertex,pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(const void*)offsetof(Vertex,tint));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(const void*)offsetof(Vertex,normal));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(const void*)offsetof(Vertex,uv));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER,0);
        ready=true;
    }
    bool ensureProgram() {
        if (program) return true;
        const char* vertexSource = R"(#version 300 es
            layout(location=0) in vec3 aPosition;
            layout(location=1) in vec3 aColour;
            layout(location=2) in vec3 aNormal;
            layout(location=3) in vec2 aUv;
            uniform mat4 uMvp;
            out vec3 vColour;
            out vec2 vUv;
            void main() {
                gl_Position=uMvp*vec4(aPosition,1.0);
                float light=0.52+0.48*abs(dot(normalize(aNormal),normalize(vec3(0.35,-0.5,0.8))));
                vColour=aColour*light;
                vUv=aUv;
            })";
        const char* fragmentSource = R"(#version 300 es
            precision mediump float;
            in vec3 vColour;
            in vec2 vUv;
            uniform sampler2D uDiffuse;
            uniform int uTextured;
            out vec4 fragColour;
            void main() {
                vec4 texel=uTextured==1?texture(uDiffuse,vUv):vec4(0.86,0.86,0.91,1.0);
                if(texel.a<0.05) discard;
                fragColour=vec4(texel.rgb*vColour,texel.a);
            })";
        auto compile=[&](GLenum type,const char* source)->GLuint {
            GLuint shader=glCreateShader(type);
            glShaderSource(shader,1,&source,nullptr);
            glCompileShader(shader);
            GLint success=GL_FALSE;glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
            if(success!=GL_TRUE) {
                char log[2048]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);errorText=log;
                glDeleteShader(shader);return 0;
            }
            return shader;
        };
        GLuint vertex=compile(GL_VERTEX_SHADER,vertexSource);
        if (!vertex) return false;
        GLuint fragment=compile(GL_FRAGMENT_SHADER,fragmentSource);
        if (!fragment) {glDeleteShader(vertex);return false;}
        program=glCreateProgram();
        glAttachShader(program,vertex);glAttachShader(program,fragment);glLinkProgram(program);
        glDeleteShader(vertex);glDeleteShader(fragment);
        GLint success=GL_FALSE;glGetProgramiv(program,GL_LINK_STATUS,&success);
        if (success!=GL_TRUE) {
            char log[2048]{};glGetProgramInfoLog(program,sizeof(log),nullptr,log);errorText=log;
            glDeleteProgram(program);program=0;return false;
        }
        return true;
    }
    bool ensureFrame(int width,int height) {
        width=std::clamp(width,1,2048);height=std::clamp(height,1,1536);
        if (framebuffer && frameWidth==width && frameHeight==height) return true;
        if (framebuffer) glDeleteFramebuffers(1,&framebuffer);
        if (colourBuffer) glDeleteTextures(1,&colourBuffer);
        if (depthBuffer) glDeleteRenderbuffers(1,&depthBuffer);
        framebuffer=colourBuffer=depthBuffer=0;
        GLint previousFramebuffer = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previousFramebuffer);
        glGenFramebuffers(1,&framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
        glGenTextures(1,&colourBuffer);
        glBindTexture(GL_TEXTURE_2D,colourBuffer);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colourBuffer,0);
        glGenRenderbuffers(1,&depthBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER,depthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,width,height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depthBuffer);
        const auto complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER,GLuint(previousFramebuffer));
        if (!complete) {errorText="Framebuffer is incomplete";return false;}
        frameWidth=width;frameHeight=height;
        return true;
    }
    void render() {
        GLint oldFramebuffer=0,oldViewport[4]{},oldProgram=0,oldVao=0,oldTexture=0,oldActiveTexture=GL_TEXTURE0;
        const GLboolean depthWasEnabled=glIsEnabled(GL_DEPTH_TEST);
        const GLboolean blendWasEnabled=glIsEnabled(GL_BLEND);
        const GLboolean cullWasEnabled=glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&oldFramebuffer);
        glGetIntegerv(GL_VIEWPORT,oldViewport);
        glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActiveTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTexture);
        const auto center=std::array<float,3>{(minimum[0]+maximum[0])*.5f,(minimum[1]+maximum[1])*.5f,(minimum[2]+maximum[2])*.5f};
        const float dimension=std::max(.01f,std::max({maximum[0]-minimum[0],maximum[1]-minimum[1],maximum[2]-minimum[2]}));
        const float aspect=float(frameWidth)/float(std::max(1,frameHeight));
        const float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
        const std::array<float,3> forward{cp*cy,cp*sy,sp};
        const std::array<float,3> right{-sy,cy,0};
        const std::array<float,3> up{-sp*cy,-sp*sy,cp};
        const float cameraDistance=dimension*distance;
        const float nearPlane=std::max(.01f,cameraDistance-dimension*1.2f);
        const float farPlane=cameraDistance+dimension*2.0f;
        const float f=1.f/std::tan(.5f*55.f*3.14159265358979323846f/180.f);
        const std::array<float,16> perspective={
            f/aspect,0,0,0,0,f,0,0,0,0,(farPlane+nearPlane)/(nearPlane-farPlane),-1,
            0,0,(2*farPlane*nearPlane)/(nearPlane-farPlane),0};
        const std::array<float,16> view={
            right[0],up[0],-forward[0],0,
            right[1],up[1],-forward[1],0,
            right[2],up[2],-forward[2],0,
            -right[0]*center[0]-right[1]*center[1]-right[2]*center[2],
            -up[0]*center[0]-up[1]*center[1]-up[2]*center[2],
            forward[0]*center[0]+forward[1]*center[1]+forward[2]*center[2]-cameraDistance,1};
        std::array<float,16> mvp{};
        for (int col=0;col<4;++col) for(int row=0;row<4;++row) for(int k=0;k<4;++k)
            mvp[col*4+row]+=perspective[k*4+row]*view[col*4+k];
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
        glViewport(0,0,frameWidth,frameHeight);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glClearColor(.075f,.085f,.12f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program,"uMvp"),1,GL_FALSE,mvp.data());
        glUniform1i(glGetUniformLocation(program,"uDiffuse"),0);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(vertexArray);
        for (const auto& range:ranges) {
            const auto texture=textures.find(range.material);
            const bool hasTexture=texture!=textures.end();
            glUniform1i(glGetUniformLocation(program,"uTextured"),hasTexture?1:0);
            glBindTexture(GL_TEXTURE_2D,hasTexture?texture->second:0);
            glDrawArrays(GL_TRIANGLES,range.first,range.count);
        }
        glBindTexture(GL_TEXTURE_2D,oldTexture);
        glActiveTexture(GLenum(oldActiveTexture));
        if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        glBindVertexArray(GLuint(oldVao));
        glUseProgram(GLuint(oldProgram));
        glBindFramebuffer(GL_FRAMEBUFFER,GLuint(oldFramebuffer));
        glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);
    }
};
