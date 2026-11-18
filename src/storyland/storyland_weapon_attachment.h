#pragma once
#include "src/leeds/leeds_model.h"
#include "src/leeds/leeds_native_world.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cctype>

float storylandPedModelForwardAngle(const StorylandModelFile& model);

inline bool storylandWeaponFirePoint(const std::array<float,16>& frame,
    const std::array<float,3>& offset,StorylandNativePoint& result) {
    if(!std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);})||
        !std::all_of(offset.begin(),offset.end(),[](float value){return std::isfinite(value);}))return false;
    for(size_t axis=0;axis<3;++axis)
        if(std::hypot(frame[axis*4],frame[axis*4+1],frame[axis*4+2])<0.000001f)return false;
    const StorylandNativePoint point{
        frame[12]+frame[0]*offset[0]+frame[4]*offset[1]+frame[8]*offset[2],
        frame[13]+frame[1]*offset[0]+frame[5]*offset[1]+frame[9]*offset[2],
        frame[14]+frame[2]*offset[0]+frame[6]*offset[1]+frame[10]*offset[2]};
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return false;
    result=point;
    return true;
}

inline bool storylandStandingPedWorldFrame(const StorylandModelFile& model,
    const StorylandNativePoint& position,float yaw,float height,std::array<float,16>& result) {
    const auto& points=model.previewPoints();
    if(points.empty()||!std::isfinite(position.x)||!std::isfinite(position.y)||
        !std::isfinite(position.z)||!std::isfinite(yaw)||!std::isfinite(height)||height<=0)return false;
    auto minimum=points.front(),maximum=minimum;
    for(const auto& point:points) {
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return false;
        minimum.x=std::min(minimum.x,point.x);maximum.x=std::max(maximum.x,point.x);
        minimum.y=std::min(minimum.y,point.y);maximum.y=std::max(maximum.y,point.y);
        minimum.z=std::min(minimum.z,point.z);maximum.z=std::max(maximum.z,point.z);
    }
    const float span=std::max({maximum.x-minimum.x,maximum.y-minimum.y,maximum.z-minimum.z});
    if(span<=0)return false;
    const float scale=std::clamp(height/std::max(.001f,span),.001f,10.0f);
    const float angle=(yaw-storylandPedModelForwardAngle(model))*.0174532925199433f;
    if(!std::isfinite(angle))return false;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    const float x=-(minimum.x+maximum.x)*.5f*scale;
    const float y=-(minimum.y+maximum.y)*.5f*scale;
    result={scale*cosine,scale*sine,0,0,-scale*sine,scale*cosine,0,0,0,0,scale,0,
        position.x+cosine*x-sine*y,position.y+sine*x+cosine*y,position.z-minimum.z*scale,1};
    return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
}

inline bool storylandPoseBoneWorldPoint(const StorylandModelFile& model,
    const std::vector<std::array<float,16>>& frames,uint32_t boneId,
    const std::array<float,16>& standingFrame,StorylandNativePoint& result) {
    const auto& bones=model.armatureBones();
    if(frames.size()!=bones.size())return false;
    const auto bone=std::find_if(bones.begin(),bones.end(),
        [boneId](const auto& value){return value.boneId==boneId;});
    if(bone==bones.end())return false;
    const auto& frame=frames[size_t(bone-bones.begin())];
    for(size_t column=0;column<3;++column) {
        const float length=std::hypot(frame[column*4],frame[column*4+1],frame[column*4+2]);
        if(!std::isfinite(length)||length<=1e-8f)return false;
    }
    const StorylandNativePoint point{
        standingFrame[12]+standingFrame[0]*frame[12]+standingFrame[4]*frame[13]+standingFrame[8]*frame[14],
        standingFrame[13]+standingFrame[1]*frame[12]+standingFrame[5]*frame[13]+standingFrame[9]*frame[14],
        standingFrame[14]+standingFrame[2]*frame[12]+standingFrame[6]*frame[13]+standingFrame[10]*frame[14]};
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return false;
    result=point;return true;
}

inline size_t storylandRightHandBone(const std::vector<StorylandModelBone>& bones) {
    size_t hand=SIZE_MAX;
    for(size_t index=0;index<bones.size();++index) {
        std::string name;
        for(unsigned char character:bones[index].name)
            if(std::isalnum(character))name.push_back(char(std::tolower(character)));
        if(name=="rhand"||name=="righthand"||name=="handr") {hand=index;break;}
    }
    if(hand==SIZE_MAX)for(size_t index=0;index<bones.size();++index)
        if(bones[index].boneId==24) {hand=index;break;}
    return hand;
}

inline bool storylandWeaponAttachmentFrame(const std::vector<StorylandModelBone>& bones,
    const std::vector<std::array<float,16>>& frames,const std::array<float,16>& standingFrame,
    std::array<float,16>& result) {
    if(frames.size()!=bones.size())return false;
    const size_t hand=storylandRightHandBone(bones);
    if(hand==SIZE_MAX)return false;
    const auto& frame=frames[hand];
    std::array<float,16> matrix{};
    for(size_t column=0;column<4;++column)for(size_t row=0;row<3;++row) {
        matrix[column*4+row]=standingFrame[row]*frame[column*4]+
            standingFrame[4+row]*frame[column*4+1]+standingFrame[8+row]*frame[column*4+2];
        if(column==3)matrix[column*4+row]+=standingFrame[12+row];
    }
    matrix[15]=1;
    if(!std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);}))return false;
    result=matrix;return true;
}

inline bool storylandNativeRootWeaponAttachmentFrame(const std::vector<StorylandModelBone>& bones,
    const std::vector<std::array<float,16>>& frames,const std::array<float,16>& nativeRoot,
    float modelForward,std::array<float,16>& result) {
    if(frames.size()!=bones.size()||!std::isfinite(modelForward))return false;
    const size_t hand=storylandRightHandBone(bones);
    if(hand==SIZE_MAX)return false;
    const float angle=(90.0f-modelForward)*0.0174532925199433f;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    std::array<float,16> oriented=frames[hand];
    for(size_t column=0;column<4;++column) {
        oriented[column*4]=cosine*frames[hand][column*4]-sine*frames[hand][column*4+1];
        oriented[column*4+1]=sine*frames[hand][column*4]+cosine*frames[hand][column*4+1];
    }
    result={};
    for(size_t column=0;column<4;++column)for(size_t row=0;row<4;++row)
        for(size_t axis=0;axis<4;++axis)result[column*4+row]+=nativeRoot[axis*4+row]*oriented[column*4+axis];
    return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
}

// Seated native animations already use the vehicle's authored scale and root.
// Match the rendered occupant transform; standing-ped centering/height scaling
// would move a motorcycle rider's head and torso away from the actual pose.
inline bool storylandNativeRootPoseBoneWorldPoint(const StorylandModelFile& model,
    const std::vector<std::array<float,16>>& frames,uint32_t boneId,
    const std::array<float,16>& nativeRoot,StorylandNativePoint& result) {
    const auto& bones=model.armatureBones();
    if(frames.size()!=bones.size()||
        !std::all_of(nativeRoot.begin(),nativeRoot.end(),[](float value){return std::isfinite(value);}))return false;
    const auto bone=std::find_if(bones.begin(),bones.end(),
        [boneId](const auto& value){return value.boneId==boneId;});
    if(bone==bones.end())return false;
    const auto& frame=frames[size_t(bone-bones.begin())];
    const float angle=(90.0f-storylandPedModelForwardAngle(model))*.0174532925199433f;
    if(!std::isfinite(angle)||!std::isfinite(frame[12])||
        !std::isfinite(frame[13])||!std::isfinite(frame[14]))return false;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    const float x=cosine*frame[12]-sine*frame[13];
    const float y=sine*frame[12]+cosine*frame[13];
    const StorylandNativePoint point{
        nativeRoot[12]+nativeRoot[0]*x+nativeRoot[4]*y+nativeRoot[8]*frame[14],
        nativeRoot[13]+nativeRoot[1]*x+nativeRoot[5]*y+nativeRoot[9]*frame[14],
        nativeRoot[14]+nativeRoot[2]*x+nativeRoot[6]*y+nativeRoot[10]*frame[14]};
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return false;
    result=point;return true;
}

// Storyland places standing models by their feet. Recover the native entity
// origin through that exact rendered transform before applying native camera
// offsets, which are relative to the entity matrix rather than its feet.
inline bool storylandPedNativeRootWorldPoint(const StorylandModelFile& model,
    const StorylandNativePoint& position,float yaw,float height,StorylandNativePoint& result) {
    const auto& points=model.previewPoints();
    if(points.empty()||!std::isfinite(position.x)||!std::isfinite(position.y)||
        !std::isfinite(position.z)||!std::isfinite(yaw)||!std::isfinite(height)||height<=0)return false;
    auto minimum=points.front(),maximum=minimum;
    for(const auto& point:points) {
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return false;
        minimum.x=std::min(minimum.x,point.x);maximum.x=std::max(maximum.x,point.x);
        minimum.y=std::min(minimum.y,point.y);maximum.y=std::max(maximum.y,point.y);
        minimum.z=std::min(minimum.z,point.z);maximum.z=std::max(maximum.z,point.z);
    }
    const float span=std::max({maximum.x-minimum.x,maximum.y-minimum.y,maximum.z-minimum.z});
    if(span<=0)return false;
    const float scale=std::clamp(height/std::max(.001f,span),.001f,10.0f);
    const float angle=(yaw-storylandPedModelForwardAngle(model))*.0174532925199433f;
    if(!std::isfinite(angle))return false;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    const float x=-(minimum.x+maximum.x)*.5f*scale;
    const float y=-(minimum.y+maximum.y)*.5f*scale;
    result={position.x+cosine*x-sine*y,position.y+sine*x+cosine*y,position.z-minimum.z*scale};
    return std::isfinite(result.x)&&std::isfinite(result.y)&&std::isfinite(result.z);
}

inline bool storylandWeaponAttachmentFrame(const std::vector<StorylandModelBone>& bones,
    const std::vector<std::array<float,16>>& frames,const std::vector<StorylandModelPoint>& points,
    const StorylandNativePoint& position,float yaw,float modelForward,std::array<float,16>& result,
    float targetHeight=1.8f) {
    if(points.empty()||frames.size()!=bones.size()||!std::isfinite(targetHeight)||targetHeight<=0)return false;
    const size_t hand=storylandRightHandBone(bones);
    if(hand==SIZE_MAX)return false;
    auto minimum=points.front(),maximum=minimum;
    for(const auto& point:points) {
        minimum.x=std::min(minimum.x,point.x);maximum.x=std::max(maximum.x,point.x);
        minimum.y=std::min(minimum.y,point.y);maximum.y=std::max(maximum.y,point.y);
        minimum.z=std::min(minimum.z,point.z);maximum.z=std::max(maximum.z,point.z);
    }
    const float scale=std::clamp(targetHeight/std::max(0.001f,std::max({maximum.x-minimum.x,
        maximum.y-minimum.y,maximum.z-minimum.z})),0.001f,10.0f);
    const float angle=(yaw-modelForward)*0.0174532925199433f;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    const auto& handFrame=frames[hand];
    result={};
    for(size_t column=0;column<3;++column) {
        result[column*4]=scale*(cosine*handFrame[column*4]-sine*handFrame[column*4+1]);
        result[column*4+1]=scale*(sine*handFrame[column*4]+cosine*handFrame[column*4+1]);
        result[column*4+2]=scale*handFrame[column*4+2];
    }
    const float x=(handFrame[12]-(minimum.x+maximum.x)*0.5f)*scale;
    const float y=(handFrame[13]-(minimum.y+maximum.y)*0.5f)*scale;
    result[12]=position.x+cosine*x-sine*y;
    result[13]=position.y+sine*x+cosine*y;
    result[14]=position.z+(handFrame[14]-minimum.z)*scale;result[15]=1;
    return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
}

inline bool storylandPoseBoneWorldFrame(const StorylandModelFile& model,
    const std::vector<std::array<float,16>>& frames,uint32_t boneId,
    const StorylandNativePoint& position,float yaw,float height,std::array<float,16>& result) {
    const auto& points=model.previewPoints();const auto& bones=model.armatureBones();
    if(points.empty()||frames.size()!=bones.size())return false;
    size_t selected=SIZE_MAX;
    for(size_t index=0;index<bones.size();++index)if(bones[index].boneId==boneId) {selected=index;break;}
    if(selected==SIZE_MAX)return false;
    auto minimum=points.front(),maximum=minimum;
    for(const auto& point:points) {
        minimum.x=std::min(minimum.x,point.x);maximum.x=std::max(maximum.x,point.x);
        minimum.y=std::min(minimum.y,point.y);maximum.y=std::max(maximum.y,point.y);
        minimum.z=std::min(minimum.z,point.z);maximum.z=std::max(maximum.z,point.z);
    }
    const float scale=std::clamp(height/std::max(.001f,std::max({maximum.x-minimum.x,
        maximum.y-minimum.y,maximum.z-minimum.z})),.001f,10.0f);
    const float angle=(yaw-storylandPedModelForwardAngle(model))*.0174532925199433f;
    const float x=(frames[selected][12]-(minimum.x+maximum.x)*.5f)*scale;
    const float y=(frames[selected][13]-(minimum.y+maximum.y)*.5f)*scale;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    std::array<float,16> matrix{};
    for(size_t column=0;column<3;++column) {
        const auto& frame=frames[selected];
        const float length=std::hypot(frame[column*4],frame[column*4+1],frame[column*4+2]);
        if(!std::isfinite(length)||length<=1e-8f)return false;
        matrix[column*4]=(cosine*frame[column*4]-sine*frame[column*4+1])/length;
        matrix[column*4+1]=(sine*frame[column*4]+cosine*frame[column*4+1])/length;
        matrix[column*4+2]=frame[column*4+2]/length;
    }
    matrix[12]=position.x+cosine*x-sine*y;
    matrix[13]=position.y+sine*x+cosine*y;
    matrix[14]=position.z+(frames[selected][14]-minimum.z)*scale;matrix[15]=1;
    if(!std::all_of(matrix.begin(),matrix.end(),[](float v){return std::isfinite(v);}))return false;
    result=matrix;return true;
}
inline bool storylandPoseBoneWorldPoint(const StorylandModelFile& model,
    const std::vector<std::array<float,16>>& frames,uint32_t boneId,
    const StorylandNativePoint& position,float yaw,float height,StorylandNativePoint& result) {
    std::array<float,16> matrix;
    if(!storylandPoseBoneWorldFrame(model,frames,boneId,position,yaw,height,matrix))return false;
    result={matrix[12],matrix[13],matrix[14]};return true;
}
