#pragma once

#include "src/leeds/leeds_model.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace StorylandModelConversion {

enum class Target {
    SanAndreasDff,
    MobileLcsDff,
    LcsPs2Mdl,
    LcsPspMdl,
    VcsPs2Mdl,
    VcsPspMdl
};

inline bool isDff(Target target) {
    return target == Target::SanAndreasDff || target == Target::MobileLcsDff;
}

inline bool isPsp(Target target) {
    return target == Target::LcsPspMdl || target == Target::VcsPspMdl;
}

inline bool isLcs(Target target) {
    return target == Target::LcsPs2Mdl || target == Target::LcsPspMdl;
}

inline const char* name(Target target) {
    switch (target) {
    case Target::SanAndreasDff: return "San Andreas DFF";
    case Target::MobileLcsDff: return "Mobile LCS DFF";
    case Target::LcsPs2Mdl: return "LCS PS2 MDL";
    case Target::LcsPspMdl: return "LCS PSP MDL";
    case Target::VcsPs2Mdl: return "VCS PS2 MDL";
    case Target::VcsPspMdl: return "VCS PSP MDL";
    }
    return "Unknown";
}

class Writer {
public:
    std::vector<uint8_t> data;

    size_t size() const { return data.size(); }
    void reserve(size_t count) { data.reserve(count); }
    size_t append(uint8_t value) { const size_t position=data.size();data.push_back(value);return position; }
    size_t append16(uint16_t value) {
        const size_t position=data.size();
        data.push_back(uint8_t(value));data.push_back(uint8_t(value >> 8));
        return position;
    }
    size_t append32(uint32_t value) {
        const size_t position=data.size();
        for(unsigned i=0;i<4;++i)data.push_back(uint8_t(value >> (i*8u)));
        return position;
    }
    size_t appendFloat(float value) {
        uint32_t word=0;std::memcpy(&word,&value,4);return append32(word);
    }
    void appendData(const std::vector<uint8_t>& bytes) {data.insert(data.end(),bytes.begin(),bytes.end());}
    void appendBytes(const uint8_t* bytes,size_t length) {data.insert(data.end(),bytes,bytes+length);}
    void appendString(const std::string& string) {
        for(char character:string)data.push_back(uint8_t(character));
        data.push_back(0);
    }
    void appendZeros(size_t count) {data.insert(data.end(),count,0);}
    void set16(size_t offset,uint16_t value) {
        data.at(offset)=uint8_t(value);data.at(offset+1)=uint8_t(value >> 8);
    }
    void set32(size_t offset,uint32_t value) {
        for(unsigned index=0;index<4;++index)data.at(offset+index)=uint8_t(value >> (index*8u));
    }
    void setFloat(size_t offset,float value) {
        uint32_t word=0;std::memcpy(&word,&value,4);set32(offset,word);
    }
    uint32_t read32(size_t offset) const {
        return uint32_t(data.at(offset))|(uint32_t(data.at(offset+1))<<8)|
            (uint32_t(data.at(offset+2))<<16)|(uint32_t(data.at(offset+3))<<24);
    }
    void align(size_t boundary) {
        if(boundary>0)appendZeros((boundary - data.size()%boundary)%boundary);
    }
    size_t allocate(size_t count,size_t alignment=4) {
        align(alignment);const size_t position=size();appendZeros(count);return position;
    }
    size_t startChunk(uint32_t type,uint32_t version) {
        const size_t position=size();append32(type);append32(0);append32(version);return position;
    }
    void endChunk(size_t position) {set32(position+4,uint32_t(size()-position-12));}
    void addChunk(uint32_t type,uint32_t version,const Writer& payload) {
        const size_t position=startChunk(type,version);appendData(payload.data);endChunk(position);
    }
};

struct Geometry {
    std::vector<uint32_t> originalVertices;
    std::vector<std::array<uint16_t,4>> triangles;
};

inline bool hasSkin(const StorylandModelFile& model) {
    for(const StorylandModelSkinWeights& vertex:model.previewSkinWeights())
        if(vertex.valid && vertex.influenceCount!=0)return true;
    return false;
}

inline bool validModel(const StorylandModelFile& model,std::string& error) {
    if(model.isEmptyDraft()||model.previewPoints().empty()||model.previewTriangles().empty()) {
        error="This model does not contain decoded triangles and vertices.";return false;
    }
    if(model.previewPoints().size()>4000000u||model.previewTriangles().size()>16000000u) {
        error="Model exceeds the converter's geometry limit.";return false;
    }
    if(hasSkin(model)) {
        error="The model has skin weights. Conversion would lose its native skeleton and animation bindings, so no file was written.";
        return false;
    }
    if(model.modelKind()==StorylandModelKind::VehicleModel) {
        error="Vehicles require their native part hierarchy, wheel pivots and damage models. A single-mesh conversion would damage the vehicle.";
        return false;
    }
    if(model.modelKind()==StorylandModelKind::CutsceneModel||model.modelKind()==StorylandModelKind::PedModel) {
        error="The character requires its native bone and animation hierarchy. Exporting only its preview mesh would be destructive.";
        return false;
    }
    for(const auto& triangle:model.previewTriangles()) {
        if(triangle.a>=model.previewPoints().size()||triangle.b>=model.previewPoints().size()||
           triangle.c>=model.previewPoints().size()){
            error="Model contains a triangle with an invalid vertex index.";return false;
        }
    }
    for(const auto& vertex:model.previewPoints()) {
        if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z)) {
            error="Model contains invalid vertex coordinates.";return false;
        }
    }
    for(const auto& animation:model.previewRslUVAnimations()) {
        if(animation.animated(0) || animation.animated(1)) {
            error="The model has RSL UV animation. Its native animation data cannot be written into a converted file.";
            return false;
        }
    }
    return true;
}

inline size_t materialCount(const StorylandModelFile& model) {
    size_t result=std::max<size_t>(1,model.previewMaterialTextureNames().size());
    for(const auto& triangle:model.previewTriangles())
        if(triangle.materialIndex!=UINT32_MAX && triangle.materialIndex<4096)
            result=std::max(result,size_t(triangle.materialIndex)+1);
    return result;
}

inline std::vector<Geometry> partition(const StorylandModelFile& model) {
    std::vector<Geometry> geometries(1);
    std::unordered_map<uint32_t,uint16_t> remap;
    for(const auto& triangle:model.previewTriangles()) {
        const uint32_t sourceVertices[3]={triangle.a,triangle.b,triangle.c};
        size_t newVertices=0;
        for(uint32_t vertex:sourceVertices)
            if(remap.find(vertex)==remap.end())++newVertices;
        if(geometries.back().originalVertices.size()+newVertices>65535u) {
            geometries.emplace_back();remap.clear();
        }
        Geometry& geometry=geometries.back();
        std::array<uint16_t,4> indices{};
        for(size_t index=0;index<3;++index){
            const uint32_t source=sourceVertices[index];
            auto existing=remap.find(source);
            if(existing==remap.end()) {
                const uint16_t local=uint16_t(geometry.originalVertices.size());
                geometry.originalVertices.push_back(source);
                remap.emplace(source,local);indices[index]=local;
            }else indices[index]=existing->second;
        }
        const size_t materials=materialCount(model);
        indices[3]=uint16_t(triangle.materialIndex<materials?triangle.materialIndex:0u);
        geometry.triangles.push_back(indices);
    }
    return geometries;
}

inline std::array<float,4> sphere(const StorylandModelFile& model,const Geometry& geometry) {
    const auto& points=model.previewPoints();
    std::array<float,4> result{};
    if(geometry.originalVertices.empty())return result;
    for(uint32_t index:geometry.originalVertices){
        const auto& p=points[index];result[0]+=p.x;result[1]+=p.y;result[2]+=p.z;
    }
    const float scale=1.0f/float(geometry.originalVertices.size());
    for(int axis=0;axis<3;++axis)result[axis]*=scale;
    for(uint32_t index:geometry.originalVertices){
        const auto& p=points[index];
        const float x=p.x-result[0],y=p.y-result[1],z=p.z-result[2];
        result[3]=std::max(result[3],std::sqrt(x*x+y*y+z*z));
    }
    return result;
}

inline void writeIdentityRwFrame(Writer& writer) {
    const float identity[12]={1,0,0,0,1,0,0,0,1,0,0,0};
    for(float v:identity)writer.appendFloat(v);
    writer.append32(0xFFFFFFFFu);
    writer.append32(0u);
}

inline void writeTexture(Writer& writer,uint32_t version,const std::string& name) {
    const size_t texture=writer.startChunk(6u,version);
    const size_t structure=writer.startChunk(1u,version);
    writer.append32(0x00001102u);
    writer.endChunk(structure);
    const size_t text=writer.startChunk(2u,version);writer.appendString(name);writer.endChunk(text);
    const size_t mask=writer.startChunk(2u,version);writer.appendString("");writer.endChunk(mask);
    const size_t ext=writer.startChunk(3u,version);writer.endChunk(ext);
    writer.endChunk(texture);
}

inline void writeMaterialList(Writer& writer,uint32_t version,const StorylandModelFile& model) {
    const size_t list=writer.startChunk(8u,version);
    const size_t structure=writer.startChunk(1u,version);
    const size_t count=materialCount(model);
    writer.append32(uint32_t(count));
    for(size_t i=0;i<count;++i)writer.append32(0xFFFFFFFFu);
    writer.endChunk(structure);
    const auto& names=model.previewMaterialTextureNames();
    const auto colors=model.previewMaterialColors();
    for(size_t i=0;i<count;++i) {
        const size_t material=writer.startChunk(7u,version);
        const size_t materialStruct=writer.startChunk(1u,version);
        writer.append32(0u);
        const uint32_t color=i<colors.size()?colors[i]:0xFFFFFFFFu;
        writer.append32(color);
        writer.append32(0u);
        const std::string texture=i<names.size()?names[i]:std::string{};
        writer.append32(texture.empty()?0u:1u);
        writer.appendFloat(1.0f);writer.appendFloat(1.0f);writer.appendFloat(1.0f);
        writer.endChunk(materialStruct);
        if(!texture.empty())writeTexture(writer,version,texture);
        const size_t extension=writer.startChunk(3u,version);
        const auto& effects=model.previewMaterialEffects();
        const auto& environmentNames=model.previewMaterialEnvironmentNames();
        if(i<effects.size() && i<environmentNames.size() &&
           effects[i]>0.0f && !environmentNames[i].empty()) {
            const size_t matfx=writer.startChunk(0x120u,version);
            writer.append32(2u);
            writer.append32(2u);
            writer.appendFloat(std::clamp(effects[i],0.0f,1.0f));
            writer.append32(0u);
            writer.append32(1u);
            writeTexture(writer,version,environmentNames[i]);
            writer.append32(0u);
            writer.endChunk(matfx);
        }
        writer.endChunk(extension);
        writer.endChunk(material);
    }
    writer.endChunk(list);
}

inline void writeRwGeometry(Writer& writer,uint32_t version,
    const StorylandModelFile& model,const Geometry& geometry) {
    const size_t rwGeometry=writer.startChunk(0x0Fu,version);
    const size_t header=writer.startChunk(1u,version);
    const auto& points=model.previewPoints();
    const auto& uv=model.previewTexcoords();
    const auto& prelights=model.previewPrelights();
    bool secondUV=false;
    bool havePrelight=false;
    bool haveNormals=true;
    for(uint32_t source:geometry.originalVertices) {
        if(source<uv.size()&&uv[source].hasSecondary)secondUV=true;
        if(source<prelights.size()&&prelights[source].valid)havePrelight=true;
        if(!points[source].hasNormal)haveNormals=false;
    }
    const bool haveUV=uv.size()==points.size();
    const uint32_t uvSets=haveUV?(secondUV?2u:1u):0u;
    const uint32_t flags=0x00000002u | 0x00000010u |
        (haveUV?(secondUV?0x00000080u:0x00000004u):0u) |
        (havePrelight?0x00000008u:0u) | (uvSets<<16u);
    writer.append32(flags);
    writer.append32(uint32_t(geometry.triangles.size()));
    writer.append32(uint32_t(geometry.originalVertices.size()));
    writer.append32(1u);
    if(version==0x00000310u) {
        writer.appendFloat(1.0f);writer.appendFloat(1.0f);writer.appendFloat(1.0f);
    }
    if(havePrelight) {
        for(uint32_t index:geometry.originalVertices) {
            const auto& c=index<prelights.size()?prelights[index]:StorylandModelPrelight{};
            writer.append(c.red);writer.append(c.green);writer.append(c.blue);writer.append(c.alpha);
        }
    }
    if(haveUV) {
        for(uint32_t index:geometry.originalVertices) {
            writer.appendFloat(uv[index].u);writer.appendFloat(uv[index].v);
        }
        if(secondUV)for(uint32_t index:geometry.originalVertices){
            writer.appendFloat(uv[index].hasSecondary?uv[index].secondaryU:uv[index].u);
            writer.appendFloat(uv[index].hasSecondary?uv[index].secondaryV:uv[index].v);
        }
    }
    for(const auto& face:geometry.triangles) {
        writer.append16(face[2]);writer.append16(face[1]);writer.append16(face[3]);writer.append16(face[0]);
    }
    for(float value:sphere(model,geometry))writer.appendFloat(value);
    writer.append32(1u);writer.append32(haveNormals?1u:0u);
    for(uint32_t index:geometry.originalVertices){
        const auto& p=points[index];writer.appendFloat(p.x);writer.appendFloat(p.y);writer.appendFloat(p.z);
    }
    if(haveNormals)for(uint32_t index:geometry.originalVertices){
        const auto& p=points[index];writer.appendFloat(p.nx);writer.appendFloat(p.ny);writer.appendFloat(p.nz);
    }
    writer.endChunk(header);
    writeMaterialList(writer,version,model);
    const size_t extension=writer.startChunk(3u,version);writer.endChunk(extension);
    writer.endChunk(rwGeometry);
}

inline bool writeDff(const StorylandModelFile& model,Target target,
        std::vector<uint8_t>& result,std::string& error) {
    if(target==Target::MobileLcsDff) {
        if(!model.isMobileLcsDff()) {
            error="Mobile LCS needs native F00D geometry, which the standard SA DFF writer cannot produce. Nothing was exported.";
            return false;
        }
        result=model.rawBytes();
        return true;
    }
    if(model.isGtaSaDff()) {
        result=model.rawBytes();
        return true;
    }
    if(!validModel(model,error))return false;
    for(const auto& uv:model.previewTexcoords()) {
        if(!std::isfinite(uv.u)||!std::isfinite(uv.v)||
           (uv.hasSecondary&&(!std::isfinite(uv.secondaryU)||!std::isfinite(uv.secondaryV)))) {
            error="Model has invalid texture coordinates.";
            return false;
        }
    }
    const auto geometries=partition(model);
    if(geometries.empty()||geometries.size()>65535u){
        error="The model has too many geometry groups for a DFF.";return false;
    }
    const uint32_t version=0x1803FFFFu;
    Writer writer;
    const size_t clump=writer.startChunk(0x10u,version);
    const size_t clumpHeader=writer.startChunk(1u,version);
    writer.append32(uint32_t(geometries.size()));
    writer.append32(0u);
    writer.append32(0u);
    writer.endChunk(clumpHeader);
    const size_t frames=writer.startChunk(0x0Eu,version);
    const size_t frameStruct=writer.startChunk(1u,version);
    writer.append32(1u);
    writeIdentityRwFrame(writer);
    writer.endChunk(frameStruct);
    const size_t frameExt=writer.startChunk(3u,version);
    const size_t frameName=writer.startChunk(0x0253F2FEu,version);
    writer.appendString("root");writer.endChunk(frameName);
    writer.endChunk(frameExt);
    writer.endChunk(frames);
    const size_t list=writer.startChunk(0x1Au,version);
    const size_t geometryCount=writer.startChunk(1u,version);
    writer.append32(uint32_t(geometries.size()));writer.endChunk(geometryCount);
    for(const Geometry& geometry:geometries)writeRwGeometry(writer,version,model,geometry);
    writer.endChunk(list);
    for(uint32_t index=0;index<geometries.size();++index) {
        const size_t atomic=writer.startChunk(0x14u,version);
        const size_t structure=writer.startChunk(1u,version);
        writer.append32(0u);
        writer.append32(index);
        writer.append32(5u);
        writer.append32(0u);
        writer.endChunk(structure);
        const size_t ext=writer.startChunk(3u,version);
        bool usesMatFx=false;
        const auto& effects=model.previewMaterialEffects();
        const auto& names=model.previewMaterialEnvironmentNames();
        for(size_t mat=0;mat<effects.size()&&mat<names.size();++mat)
            if(effects[mat]>0.0f&&!names[mat].empty())usesMatFx=true;
        if(usesMatFx) {
            const size_t plugin=writer.startChunk(0x120u,version);
            writer.append32(1u);
            writer.endChunk(plugin);
        }
        writer.endChunk(ext);
        writer.endChunk(atomic);
    }
    const size_t extension=writer.startChunk(3u,version);writer.endChunk(extension);
    writer.endChunk(clump);
    result=std::move(writer.data);
    return true;
}

}
