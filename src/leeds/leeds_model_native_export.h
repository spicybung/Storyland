#pragma once

#include "src/leeds/leeds_model_conversion.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace StorylandModelConversion {

inline bool finiteUv(const StorylandModelFile& model,std::string& error) {
    for (const auto& material : model.previewMaterialUVAnimations()) {
        if (material.animated(0u) || material.animated(1u)) {
            error="The DFF has UV animation. The target MDL writer cannot yet retain it; no file was written.";
            return false;
        }
    }
    for(const auto& uv:model.previewTexcoords()) {
        if(!std::isfinite(uv.u)||!std::isfinite(uv.v)||
           (uv.hasSecondary&&(!std::isfinite(uv.secondaryU)||!std::isfinite(uv.secondaryV)))) {
            error="Model has invalid texture coordinates.";return false;
        }
    }
    if(model.armatureBones().size()>1u ||
       std::any_of(model.previewTriangles().begin(),model.previewTriangles().end(),
                   [](const StorylandModelTriangle& tri){return !tri.vehiclePart.empty() && tri.vehiclePart!="root";})) {
        error="Model has multiple frames or attached parts. A single-atomic export would lose their hierarchy.";
        return false;
    }
    if(materialCount(model)>512u) {
        error="The native material list exceeds 512 entries.";return false;
    }
    return true;
}

inline int16_t quantizeI16(float input) {
    const double value=std::round(double(input));
    return int16_t(std::max(-32768.0,std::min(32767.0,value)));
}
inline int8_t quantizeI8(float input) {
    const double value=std::round(double(input));
    return int8_t(std::max(-128.0,std::min(127.0,value)));
}
inline uint8_t quantizeU8(float input) {
    const double value=std::round(double(input));
    return uint8_t(std::max(0.0,std::min(255.0,value)));
}
inline uint16_t color5551(const StorylandModelPrelight& light) {
    return uint16_t((uint32_t(light.red)*31u/255u) |
        (uint32_t(light.green)*31u/255u)<<5u |
        (uint32_t(light.blue)*31u/255u)<<10u |
        (light.alpha>=128u?0x8000u:0u));
}
inline std::array<float,3> positionCenter(const StorylandModelFile& model) {
    std::array<float,3> minimum={std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    std::array<float,3> maximum={-minimum[0],-minimum[1],-minimum[2]};
    for(const auto& point:model.previewPoints()) {
        const float value[3]={point.x,point.y,point.z};
        for(unsigned axis=0;axis<3u;++axis) {
            minimum[axis]=std::min(minimum[axis],value[axis]);
            maximum[axis]=std::max(maximum[axis],value[axis]);
        }
    }
    std::array<float,3> result{};
    for(unsigned axis=0;axis<3u;++axis)result[axis]=(minimum[axis]+maximum[axis])*0.5f;
    return result;
}
inline std::array<float,3> positionHalfExtent(const StorylandModelFile& model,const std::array<float,3>& center) {
    std::array<float,3> extent{};
    for(const auto& point:model.previewPoints()) {
        const float value[3]={point.x,point.y,point.z};
        for(unsigned axis=0;axis<3u;++axis)extent[axis]=std::max(extent[axis],std::fabs(value[axis]-center[axis]));
    }
    for(float& value:extent)value=std::max(value,1.0e-6f);
    return extent;
}

class NativeResourceWriter : public Writer {
public:
    std::vector<uint32_t> pointers;
    std::vector<std::pair<uint32_t,std::string>> textureFields;
    NativeResourceWriter() {
        appendBytes(reinterpret_cast<const uint8_t*>("ldm\0"),4u);
        appendZeros(0x24u);
    }
    uint32_t allocate32(size_t length,size_t alignment=4u) {
        const size_t result=allocate(length,alignment);
        if(result>UINT32_MAX)throw std::length_error("Native model exceeds the maximum offset size.");
        return uint32_t(result);
    }
    void pointer(uint32_t field,uint32_t target) {
        set32(field,target);
        if(target && std::find(pointers.begin(),pointers.end(),field)==pointers.end())
            pointers.push_back(field);
    }
    uint32_t cstring(const std::string& name) {
        const uint32_t target=allocate32(0,1);
        appendString(name);
        return target;
    }
    void nativeMatrix(uint32_t offset) {
        for(unsigned column=0;column<4u;++column)for(unsigned row=0;row<4u;++row)
            setFloat(offset+column*16u+row*4u,column==row?1.0f:0.0f);
    }
    void finish(uint32_t atomic,bool psp) {
        align(4u);
        const uint32_t relocationTable=uint32_t(size());
        const uint32_t relocationCount=uint32_t(pointers.size()+textureFields.size()+1u);
        const uint32_t textureHead=relocationTable+relocationCount*4u;
        const uint32_t stringStart=textureHead+4u;
        uint32_t stringCursor=stringStart;
        for(const auto& texture:textureFields) {
            set32(texture.first,stringCursor);
            stringCursor+=uint32_t(texture.second.size()+1u);
        }
        append32(textureHead);
        for(uint32_t field:pointers)append32(field);
        for(const auto& texture:textureFields)append32(texture.first);
        if(size()!=textureHead)throw std::logic_error("Native relocation table size mismatch.");
        append32(atomic+0x24u);
        for(const auto& texture:textureFields)appendString(texture.second);
        if(size()!=stringCursor)throw std::logic_error("Native texture table size mismatch.");
        set32(4u,0u);
        set32(8u,uint32_t(size()));
        set32(12u,relocationTable);
        set32(16u,relocationTable);
        set32(20u,relocationCount);
        set32(24u,textureHead);
        set32(28u,0x10000u);
        align(2048u);
        (void)psp;
    }

};

inline void writeNativeMaterialEnvironment(NativeResourceWriter& writer,
                                        uint32_t materialField,
                                        const StorylandModelFile& model,
                                        size_t index) {
    const auto& strengths=model.previewMaterialEffects();
    const auto& environmentNames=model.previewMaterialEnvironmentNames();
    if(index>=strengths.size() || index>=environmentNames.size() ||
       strengths[index]<=0.0f || environmentNames[index].empty())return;
    const uint32_t effect=writer.allocate32(16u);
    writer.setFloat(effect+8u,std::clamp(strengths[index],0.0f,1.0f));
    writer.set32(effect+12u,2u);
    writer.textureFields.emplace_back(effect+4u,environmentNames[index]);
    writer.pointer(materialField+12u,effect);
}

inline bool writeNativePsp(const StorylandModelFile& model,Target target,
                           std::vector<uint8_t>& result,std::string& error) {
    if(!validModel(model,error)||!finiteUv(model,error))return false;
    const auto& points=model.previewPoints();
    const auto& triangles=model.previewTriangles();
    const auto& uv=model.previewTexcoords();
    const auto& colors=model.previewPrelights();
    const size_t materials=materialCount(model);
    if(triangles.size()>512u) {
        error="This PSP SimpleModel export supports up to 512 native triangle strips. Split the model into multiple atomics before exporting.";
        return false;
    }
    const uint32_t stripCount=uint32_t(triangles.size());
    const uint32_t vertexCount=stripCount*3u;
    const bool haveUv=uv.size()==points.size();
    const bool haveColors=colors.size()==points.size()&&
        std::any_of(colors.begin(),colors.end(),[](const StorylandModelPrelight& c){return c.valid;});
    const bool haveNormals=std::all_of(points.begin(),points.end(),
        [](const StorylandModelPoint& p){return p.hasNormal;});
    const uint32_t flags=(2u<<7u)|(haveUv?3u:0u)|(haveColors?(7u<<2u):0u)|
        (haveNormals?(1u<<5u):0u);
    const uint32_t uvOffset=0u;
    const uint32_t colorOffset=haveUv?8u:0u;
    const uint32_t normalOffset=colorOffset+(haveColors?4u:0u);
    const uint32_t positionOffset=(normalOffset+(haveNormals?3u:0u)+1u)&~1u;
    const uint32_t stride=(positionOffset+6u+3u)&~3u;
    const auto center=positionCenter(model);
    const auto extent=positionHalfExtent(model,center);
    std::array<float,3> scale{};
    for(unsigned axis=0;axis<3u;++axis)scale[axis]=extent[axis]*(32768.0f/32760.0f);
    float radius=0.0f;
    for(const auto& point:points) {
        const float dx=point.x-center[0],dy=point.y-center[1],dz=point.z-center[2];
        radius=std::max(radius,std::sqrt(dx*dx+dy*dy+dz*dz));
    }
    NativeResourceWriter writer;
    const uint32_t frame=writer.allocate32(0xB0,16u);
    writer.set32(frame,0x300u);
    writer.nativeMatrix(frame+0x10u);
    writer.nativeMatrix(frame+0x50u);
    writer.pointer(frame+0x08u,frame+0x08u);
    writer.pointer(frame+0x0Cu,frame+0x08u);
    writer.pointer(frame+0x98u,frame);
    writer.pointer(frame+0xA8u,writer.cstring("root"));

    const uint32_t atomic=writer.allocate32(0x34u,16u);
    writer.set32(atomic,isLcs(target)?0x01050001u:0x00041601u);
    writer.pointer(atomic+4u,frame);
    writer.pointer(atomic+8u,frame+8u);
    writer.pointer(atomic+12u,frame+8u);
    writer.pointer(atomic+28u,atomic+28u);
    writer.pointer(atomic+32u,atomic+28u);
    writer.set32(atomic+36u,0u);
    writer.set32(atomic+40u,0x0000FFFFu);
    writer.pointer(0x20u,atomic);

    const uint32_t geometry=writer.allocate32(0x20u,16u);
    writer.set32(geometry,8u);
    writer.set32(geometry+8u,1u);
    writer.pointer(atomic+20u,geometry);
    const uint32_t header=writer.allocate32(0x48u,1u);
    const uint32_t vertexData=(0x48u+stripCount*0x30u+15u)&~15u;
    const uint32_t bufferSize=vertexData+vertexCount*stride;
    writer.appendZeros(bufferSize-0x48u);
    writer.set32(header+0u,bufferSize);
    writer.set32(header+4u,flags);
    writer.set32(header+8u,stripCount);
    for(unsigned axis=0;axis<3u;++axis) {
        writer.setFloat(header+0x10u+axis*4u,center[axis]);
        writer.setFloat(header+0x20u+axis*4u,scale[axis]);
        writer.setFloat(header+0x30u+axis*4u,center[axis]);
    }
    writer.setFloat(header+0x1Cu,radius);
    writer.set32(header+0x2Cu,vertexCount);
    writer.set32(header+0x40u,vertexData);
    for(uint32_t index=0u;index<stripCount;++index) {
        const auto& face=triangles[index];
        const uint32_t ids[3]={face.a,face.b,face.c};
        const uint32_t part=header+0x48u+index*0x30u;
        writer.set32(part,index*3u*stride);
        writer.set16(part+4u,1u);
        writer.set16(part+6u,uint16_t(face.materialIndex<materials?face.materialIndex:0u));
        writer.setFloat(part+12u,1.0f);writer.setFloat(part+16u,1.0f);
        float minU=std::numeric_limits<float>::max(),minV=minU;
        float maxU=-minU,maxV=-minV;
        for(unsigned corner=0;corner<3u;++corner) {
            const uint32_t source=ids[corner];
            const uint32_t entry=header+vertexData+(index*3u+corner)*stride;
            if(haveUv) {
                writer.setFloat(entry+uvOffset,uv[source].u);
                writer.setFloat(entry+uvOffset+4u,uv[source].v);
                minU=std::min(minU,uv[source].u);minV=std::min(minV,uv[source].v);
                maxU=std::max(maxU,uv[source].u);maxV=std::max(maxV,uv[source].v);
            }
            if(haveColors) {
                const auto& light=colors[source];
                writer.data.at(entry+colorOffset)=light.red;
                writer.data.at(entry+colorOffset+1)=light.green;
                writer.data.at(entry+colorOffset+2)=light.blue;
                writer.data.at(entry+colorOffset+3)=light.alpha;
            }
            if(haveNormals) {
                writer.data.at(entry+normalOffset)=uint8_t(quantizeI8(points[source].nx*127.0f));
                writer.data.at(entry+normalOffset+1u)=uint8_t(quantizeI8(points[source].ny*127.0f));
                writer.data.at(entry+normalOffset+2u)=uint8_t(quantizeI8(points[source].nz*127.0f));
            }
            const float xyz[3]={points[source].x,points[source].y,points[source].z};
            for(unsigned axis=0;axis<3u;++axis)
                writer.set16(entry+positionOffset+axis*2u,
                    uint16_t(quantizeI16((xyz[axis]-center[axis])/scale[axis]*32768.0f)));
        }
        writer.setFloat(part+0x14u,haveUv?minU:0.0f);
        writer.setFloat(part+0x18u,haveUv?minV:0.0f);
        writer.setFloat(part+0x1Cu,haveUv?maxU:0.0f);
        writer.setFloat(part+0x20u,haveUv?maxV:0.0f);
    }

    const auto& textures=model.previewMaterialTextureNames();
    const auto matColors=model.previewMaterialColors();
    std::vector<uint32_t> descriptors;
    for(size_t i=0;i<materials;++i) {
        const uint32_t entry=writer.allocate32(16u);
        writer.set32(entry+4u,i<matColors.size()?matColors[i]:0xFFFFFFFFu);
        writer.set32(entry+8u,1u);
        if(i<textures.size()&&!textures[i].empty())writer.textureFields.emplace_back(entry,textures[i]);
        writeNativeMaterialEnvironment(writer,entry,model,i);
        descriptors.push_back(entry);
    }
    const uint32_t table=writer.allocate32(materials*4u);
    for(size_t i=0;i<materials;++i)writer.pointer(table+uint32_t(i)*4u,descriptors[i]);
    writer.pointer(geometry+0x0Cu,table);
    writer.set32(geometry+0x10u,uint32_t(materials));
    writer.set32(geometry+0x14u,uint32_t(materials));
    writer.finish(atomic,true);
    result=std::move(writer.data);
    return true;
}

inline void writePs2VifWord(Writer& writer,uint32_t word){writer.append32(word);}
inline void makePs2StripPacket(Writer& out,const StorylandModelFile& model,
                               const StorylandModelTriangle& triangle,
                               const std::array<float,3>& center,
                               const std::array<float,3>& scale) {
    const uint32_t ids[3]={triangle.a,triangle.b,triangle.c};
    const auto& points=model.previewPoints();
    const auto& texcoords=model.previewTexcoords();
    const auto& colors=model.previewPrelights();
    const bool haveUv=texcoords.size()==points.size();
    Writer packet;
    packet.append32(0x6C018000u);
    packet.append32(0u);packet.append32(0u);packet.append32(3u);packet.append32(0x8003u);
    packet.append32(0x20000000u);packet.append32(0x40404040u);
    packet.append32(0x30000000u);
    packet.append32(0u);packet.append32(0u);packet.append32(0u);packet.append32(0u);
    packet.append32(0x79038001u);
    for(uint32_t source:ids) {
        const auto& p=points[source];const float xyz[3]={p.x,p.y,p.z};
        for(unsigned axis=0;axis<3u;++axis)
            packet.append16(uint16_t(quantizeI16((xyz[axis]-center[axis])/(scale[axis]*0.00003051820313464149f))));
    }
    packet.align(4u);
    packet.append32(0x20000000u);packet.append32(0x50505050u);
    packet.append32(0x30000000u);
    packet.append32(0u);packet.append32(0u);packet.append32(0xFFu);packet.append32(0u);
    packet.append32(0x7603C048u);
    for(uint32_t source:ids) {
        const float u=haveUv?texcoords[source].u:0.0f;
        const float v=haveUv?texcoords[source].v:0.0f;
        packet.append(quantizeU8(u*127.5f));packet.append(quantizeU8((1.0f-v)*127.5f));
    }
    packet.align(4u);
    packet.append32(0x6F03808Fu);
    for(uint32_t source:ids) {
        const auto& color=(source<colors.size()?colors[source]:StorylandModelPrelight{});
        packet.append16(color5551(color));
    }
    packet.align(4u);
    packet.append32(0x14000000u);
    packet.align(16u);
    out.append32(0x60000000u | uint32_t(packet.size()/16u));
    out.appendZeros(12u);
    out.appendData(packet.data);
}

inline bool writeNativePs2(const StorylandModelFile& model,Target target,
                           std::vector<uint8_t>& result,std::string& error) {
    if(!validModel(model,error)||!finiteUv(model,error))return false;
    const auto& points=model.previewPoints();
    const auto& triangles=model.previewTriangles();
    const size_t materials=materialCount(model);
    if(triangles.size()>4095u||triangles.size()*3u>65535u) {
        error="PS2 SimpleModel geometry exceeds the native strip or 16-bit vertex limits.";
        return false;
    }
    NativeResourceWriter writer;
    writer.appendZeros(0x180u-size_t(writer.size()));
    const uint32_t frame=0x30u,atomic=0xE0u,geometry=0x120u,world=0x140u;
    const uint32_t parts=uint32_t(triangles.size());
    const uint32_t dmaStart=(0x180u+parts*0x30u+15u)&~15u;
    writer.appendZeros(dmaStart-size_t(writer.size()));
    writer.set32(frame,0x0300AA00u);
    writer.pointer(frame+0x08u,atomic+0x08u);
    writer.pointer(frame+0x0Cu,atomic+0x08u);
    writer.pointer(frame+0x98u,frame);
    writer.nativeMatrix(0x40u);
    writer.nativeMatrix(0x80u);
    writer.set32(atomic,0x0004AA01u);
    writer.pointer(atomic+4u,frame);
    writer.pointer(atomic+8u,frame+8u);
    writer.pointer(atomic+12u,frame+8u);
    writer.pointer(atomic+20u,geometry);
    writer.pointer(0x20u,atomic);
    writer.set32(geometry,8u);writer.set32(geometry+8u,1u);
    const auto center=positionCenter(model);
    const auto extent=positionHalfExtent(model,center);
    constexpr float globalScale=0.00003051820313464149f;
    std::array<float,3> scale{};
    for(unsigned axis=0;axis<3u;++axis)scale[axis]=extent[axis]/(28672.0f*globalScale);
    for(unsigned axis=0;axis<3u;++axis) {
        writer.setFloat(world+0x28u+axis*4u,scale[axis]);
        writer.setFloat(world+0x34u+axis*4u,center[axis]);
    }
    float radius=0.0f;
    for(const auto& point:points) {
        const float x=point.x-center[0],y=point.y-center[1],z=point.z-center[2];
        radius=std::max(radius,std::sqrt(x*x+y*y+z*z));
    }
    for(unsigned axis=0;axis<3u;++axis)writer.setFloat(world+axis*4u,center[axis]);
    writer.setFloat(world+0x0Cu,radius);
    writer.set32(world+0x14u,0x0Du);
    writer.set16(world+0x18u,uint16_t(parts*3u));
    writer.set16(world+0x1Au,uint16_t(dmaStart-world));
    const auto& texcoords=model.previewTexcoords();
    for(uint32_t index=0;index<parts;++index) {
        const auto& face=triangles[index];
        const uint32_t part=0x180u+index*0x30u;
        const Geometry geometry{{face.a,face.b,face.c},{{0,1,2,0}}};
        const auto bound=sphere(model,geometry);
        for(unsigned axis=0;axis<4u;++axis)writer.setFloat(part+axis*4u,bound[axis]);
        writer.setFloat(part+0x10u,1.0f);writer.setFloat(part+0x14u,1.0f);
        writer.set32(part+0x18u,0x19u);
        writer.set32(part+0x1Cu,uint32_t(writer.size()-dmaStart));
        writer.set16(part+0x20u,3u);
        writer.set16(part+0x22u,uint16_t(face.materialIndex<materials?face.materialIndex:0u));
        makePs2StripPacket(writer,model,face,center,scale);
    }
    const uint32_t nativeSize=uint32_t(writer.size()-world);
    if(nativeSize>0xFFFFFu){error="Native PS2 DMA data exceeds the resource length field.";return false;}
    writer.set32(world+0x10u,(parts<<20u)|nativeSize);
    const auto& names=model.previewMaterialTextureNames();
    const auto matColors=model.previewMaterialColors();
    std::vector<uint32_t> descriptors;
    for(size_t i=0;i<materials;++i) {
        const uint32_t entry=writer.allocate32(16u);
        writer.set32(entry+4u,i<matColors.size()?matColors[i]:0xFFFFFFFFu);
        writer.set32(entry+8u,2u);
        if(i<names.size()&&!names[i].empty())writer.textureFields.emplace_back(entry,names[i]);
        writeNativeMaterialEnvironment(writer,entry,model,i);
        descriptors.push_back(entry);
    }
    const uint32_t table=writer.allocate32(materials*4u);
    for(size_t i=0;i<materials;++i)writer.pointer(table+uint32_t(i)*4u,descriptors[i]);
    writer.pointer(geometry+12u,table);
    writer.set32(geometry+16u,uint32_t(materials));
    writer.set32(geometry+20u,0x10u);
    writer.finish(atomic,false);
    result=std::move(writer.data);
    return true;
}

inline bool writeMobileLcsDff(const StorylandModelFile& model,
                           std::vector<uint8_t>& result,std::string& error) {
    if(model.isMobileLcsDff()) {result=model.rawBytes();return true;}
    if(!validModel(model,error)||!finiteUv(model,error))return false;
    const auto& points=model.previewPoints();
    const auto& faces=model.previewTriangles();
    const auto& names=model.previewMaterialTextureNames();
    const size_t materials=materialCount(model);
    if(faces.size()>4095u||faces.size()*3u>65535u) {
        error="Mobile LCS native geometry is limited to 4095 strips and 65535 strip vertices.";
        return false;
    }
    const auto center=positionCenter(model);
    const auto extent=positionHalfExtent(model,center);
    constexpr float globalScale=0.00003051820313464149f;
    std::array<float,3> scale{};
    for(unsigned axis=0;axis<3u;++axis)scale[axis]=extent[axis]/(28672.0f*globalScale);
    Writer writer;
    constexpr uint32_t rwVersion=0x310u;
    const size_t clump=writer.startChunk(0x10u,rwVersion);
    const size_t clumpStruct=writer.startChunk(1u,rwVersion);
    writer.append32(1u);
    writer.endChunk(clumpStruct);
    const size_t frames=writer.startChunk(0x0Eu,rwVersion);
    const size_t frameData=writer.startChunk(1u,rwVersion);
    writer.append32(1u);
    writeIdentityRwFrame(writer);
    writer.endChunk(frameData);
    const size_t frameExt=writer.startChunk(3u,rwVersion);
    const size_t frameName=writer.startChunk(0x0253F2FEu,rwVersion);
    writer.appendString("root");writer.endChunk(frameName);
    writer.endChunk(frameExt);
    writer.endChunk(frames);
    const size_t atomic=writer.startChunk(0x14u,rwVersion);
    const size_t atomicStruct=writer.startChunk(1u,rwVersion);
    writer.append32(0u);
    writer.append32(5u);
    writer.append32(0u);
    writer.endChunk(atomicStruct);
    const size_t geometry=writer.startChunk(0x0Fu,0xF00Du);
    const size_t body=writer.size();
    const size_t partCount=faces.size();
    const size_t dmaOffset=(0x40u+partCount*0x30u+15u)&~15u;
    writer.appendZeros(dmaOffset);
    float radius=0.0f;
    for(const auto& point:points) {
        const float x=point.x-center[0],y=point.y-center[1],z=point.z-center[2];
        radius=std::max(radius,std::sqrt(x*x+y*y+z*z));
    }
    for(unsigned axis=0;axis<3u;++axis)writer.setFloat(body+axis*4u,center[axis]);
    writer.setFloat(body+0x0Cu,radius);
    writer.set32(body+0x14u,0x0Du);
    writer.set16(body+0x18u,uint16_t(partCount*3u));
    writer.set16(body+0x1Au,uint16_t(dmaOffset));
    for(unsigned axis=0;axis<3u;++axis) {
        writer.setFloat(body+0x28u+axis*4u,scale[axis]);
        writer.setFloat(body+0x34u+axis*4u,center[axis]);
    }
    for(size_t index=0;index<faces.size();++index) {
        const auto& face=faces[index];
        const size_t header=body+0x40u+index*0x30u;
        const Geometry selected{{face.a,face.b,face.c},{{0,1,2,0}}};
        const auto bounds=sphere(model,selected);
        for(unsigned axis=0;axis<4u;++axis)writer.setFloat(header+axis*4u,bounds[axis]);
        writer.setFloat(header+0x10u,1.0f);
        writer.setFloat(header+0x14u,1.0f);
        writer.set32(header+0x18u,0x19u);
        writer.set32(header+0x1Cu,uint32_t(writer.size()-(body+dmaOffset)));
        writer.set16(header+0x20u,1u);
        writer.set16(header+0x22u,uint16_t(face.materialIndex<materials?face.materialIndex:0u));
        const size_t packetStart=writer.size();
        makePs2StripPacket(writer,model,face,center,scale);
        for(size_t offset=packetStart+16u;offset+4u<=writer.size();offset+=4u){
            if(writer.read32(offset)==0x14000000u) {
                writer.set32(offset,0x14000006u);
                break;
            }
        }
    }
    const size_t nativeSize=writer.size()-body;
    if(nativeSize>0xFFFFFu){error="Mobile LCS native payload exceeds the geometry size field.";return false;}
    writer.set32(body+0x10u,(uint32_t(partCount)<<20u)|uint32_t(nativeSize));
    writeMaterialList(writer,rwVersion,model);
    writer.endChunk(geometry);
    const size_t atomicExt=writer.startChunk(3u,rwVersion);
    const auto& effects=model.previewMaterialEffects();
    const auto& environmentNames=model.previewMaterialEnvironmentNames();
    for(size_t index=0;index<effects.size()&&index<environmentNames.size();++index) {
        if(effects[index]>0.0f&&!environmentNames[index].empty()) {
            const size_t plugin=writer.startChunk(0x120u,rwVersion);
            writer.append32(1u);
            writer.endChunk(plugin);
            break;
        }
    }
    writer.endChunk(atomicExt);
    writer.endChunk(atomic);
    const size_t clumpExt=writer.startChunk(3u,rwVersion);writer.endChunk(clumpExt);
    writer.endChunk(clump);
    result=std::move(writer.data);
    return true;
}

inline bool convert(const StorylandModelFile& model,Target target,
                    std::vector<uint8_t>& output,std::string& error) {
    output.clear();error.clear();
    if(target==Target::MobileLcsDff)return writeMobileLcsDff(model,output,error);
    if(isDff(target))return writeDff(model,target,output,error);
    if(isPsp(target))return writeNativePsp(model,target,output,error);
    return writeNativePs2(model,target,output,error);
}

}
