#include "src/storyland/storyland_cutscene.h"
#include "src/leeds/leeds_cutscene_dat.h"
#include "src/storyland/storyland_mobile_animation.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_intro_actor_policy.h"
#include "src/storyland/storyland_cutscene_rigid_prop.h"
#include <cmath>
#include <cstring>
#include <sstream>

namespace {
std::string cutsceneLower(std::string value) {
    for(char& character:value)character=char(std::tolower(static_cast<unsigned char>(character)));
    return value;
}
std::string cutsceneTrim(const std::string& value) {
    const auto first=value.find_first_not_of(" \t\r\n");
    if(first==std::string::npos)return {};
    return value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
}
}

void StorylandCutscene::clear() {
    loaded=false;nativeLcs=false;nativeLcsPs2=false;duration=0;actorVerticalOffset=0;offset={};actorRows.clear();subtitleRows.clear();
    for(auto& spline:splines)spline.clear();
}

bool StorylandCutscene::loadDefinition(const std::vector<uint8_t>& bytes,std::vector<std::string>& models,
    std::vector<std::string>& clips,std::vector<bool>& rootMotion,bool lcs,std::string& error) {
    if(bytes.empty()||bytes.size()>4u*1024u*1024u) {
        error="Cutscene definition length is invalid.";
        return false;
    }
    std::string text;
    if(bytes.size()>=32&&std::memcmp(bytes.data(),"tuc",3)==0) {
        uint32_t size=0;
        std::memcpy(&size,bytes.data()+8,4);
        if(size<32||size>bytes.size()||size>1024u*1024u) {
            error="Cutscene native CUT header length is invalid.";
            return false;
        }
        text.assign(reinterpret_cast<const char*>(bytes.data()+32),size-32);
    } else {
        text.assign(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    }
    const auto terminator=text.find('\0');if(terminator!=std::string::npos)text.resize(terminator);
    std::istringstream lines(text);
    std::string line,section;
    std::map<std::string,bool> motion;
    bool hasOffset=false;
    while(std::getline(lines,line)) {
        line=cutsceneTrim(line);
        if(line.empty())continue;
        const auto lower=cutsceneLower(line);
        if(lower=="end") {section.clear();continue;}
        if(section.empty()) {section=lower;continue;}
        if(section=="info") {
            std::istringstream fields(line);std::string key;fields>>key;
            if(cutsceneLower(key)=="offset") {
                if(!(fields>>offset.x>>offset.y>>offset.z)||!std::isfinite(offset.x)||
                    !std::isfinite(offset.y)||!std::isfinite(offset.z)) {error="Cutscene offset is invalid.";return false;}
                hasOffset=true;
            }
        } else if(section=="model") {
            std::replace(line.begin(),line.end(),',',' ');
            std::istringstream fields(line);int type=0;std::string model,clip;
            if(!(fields>>type>>model>>clip)||models.size()>=128||model.size()>64||clip.size()>64) {
                error="Cutscene model declaration is invalid.";return false;
            }
            models.push_back(model);clips.push_back(clip);
        } else if(section=="text") {
            if(lcs)std::replace(line.begin(),line.end(),',',' ');
            std::istringstream fields(line);Subtitle subtitle;
            int64_t start=0,endOrDuration=0;
            if(!(fields>>start>>endOrDuration>>subtitle.key)||start<0||endOrDuration<0||
                start>3600000||endOrDuration>3600000||subtitle.key.size()>64||
                subtitleRows.size()>=4096) {error="Cutscene subtitle timing is invalid.";return false;}
            const int64_t end=lcs?start+endOrDuration:endOrDuration;
            if(end<start||end>3600000) {error="Cutscene subtitle timing is invalid.";return false;}
            subtitle.start=uint32_t(start);subtitle.end=uint32_t(end);
            std::getline(fields,subtitle.text);subtitle.text=cutsceneTrim(subtitle.text);
            const auto opening=subtitle.text.find('{'),closing=subtitle.text.find_last_of('}');
            if(opening!=std::string::npos&&closing>opening)subtitle.text=subtitle.text.substr(opening+1,closing-opening-1);
            const auto voice=subtitle.text.find(']');
            const auto voiceStart=subtitle.text.find(':');
            if(voice!=std::string::npos&&voiceStart<voice) {
                subtitle.audioStream=cutsceneTrim(subtitle.text.substr(voiceStart+1,voice-voiceStart-1));
                subtitle.text.erase(0,voice+1);
            }
            subtitle.text=cutsceneTrim(subtitle.text);subtitleRows.push_back(std::move(subtitle));
        } else if(section=="motion") {
            const auto colon=line.find(':');
            if(colon!=std::string::npos) {
                const auto comma=line.find(',',colon);
                if(comma!=std::string::npos)motion[cutsceneLower(line.substr(0,colon))]=cutsceneTrim(line.substr(comma+1))=="1";
            }
        }
    }
    if(!hasOffset||models.empty()) {error="Cutscene has no valid origin or actor declarations.";return false;}
    for(const auto& clip:clips)rootMotion.push_back(motion[cutsceneLower(clip)]);
    return true;
}

bool StorylandCutscene::loadCamera(const std::vector<uint8_t>& bytes,std::string& error) {
    if(bytes.empty()) {error="Native cutscene has no camera resource.";return false;}
    if(bytes.size()<48||std::memcmp(bytes.data(),"MAC",3)!=0) {
        std::array<std::vector<StorylandCutsceneDatKey>,4> decoded;
        if(!storylandDecodeCutsceneDat(bytes,decoded,error))return false;
        duration=0;
        for(size_t index=0;index<4;++index) {
            splines[index].clear();
            for(const auto& source:decoded[index]) {
                SplineKey key;
                key.time=source.time;
                key.value=source.value;
                key.incoming=source.incoming;
                key.outgoing=source.outgoing;
                splines[index].push_back(key);
            }
            duration=std::max(duration,splines[index].back().time);
        }
        if(duration<=0) {error="Native cutscene DAT has no timed camera movement.";return false;}
        error.clear();
        return true;
    }
    const auto word=[&](size_t index) {uint32_t value=0;std::memcpy(&value,bytes.data()+index,4);return value;};
    const auto real=[&](size_t index) {float value=0;std::memcpy(&value,bytes.data()+index,4);return value;};
    const uint32_t size=word(8);
    if(size>bytes.size()||size<48) {error="Cutscene camera resource is truncated.";return false;}
    for(size_t spline=0;spline<4;++spline) {
        const uint32_t start=word(32+spline*4);
        if(start<48||start>size-4) {error="Cutscene spline pointer is outside its resource.";return false;}
        const float countValue=real(start);
        if(!std::isfinite(countValue)||countValue<1||countValue>4096||std::floor(countValue)!=countValue) {
            error="Cutscene spline key count is invalid.";return false;
        }
        const size_t count=size_t(countValue),dimensions=spline<2?1:3,stride=4*(1+dimensions*3);
        if(count>(size-start-4)/stride) {error="Cutscene spline keys exceed their resource.";return false;}
        for(size_t index=0;index<count;++index) {
            const size_t position=size_t(start)+4+index*stride;
            SplineKey key;key.time=real(position);
            if(!std::isfinite(key.time)||key.time<0||key.time>3600||
                (!splines[spline].empty()&&key.time<splines[spline].back().time)) {
                error="Cutscene spline key times are invalid.";return false;
            }
            for(size_t axis=0;axis<dimensions;++axis) {
                key.incoming[axis]=real(position+4+axis*4);
                key.value[axis]=real(position+4+(dimensions+axis)*4);
                key.outgoing[axis]=real(position+4+(dimensions*2+axis)*4);
                if(!std::isfinite(key.incoming[axis])||!std::isfinite(key.value[axis])||!std::isfinite(key.outgoing[axis])) {
                    error="Cutscene spline contains a non-finite control point.";return false;
                }
            }
            splines[spline].push_back(key);
        }
        duration=std::max(duration,splines[spline].back().time);
    }
    return duration>0;
}

std::array<float,3> StorylandCutscene::sampleSpline(size_t spline,float seconds) const {
    const auto& keys=splines[spline];
    if(keys.empty())return {};
    if(seconds<keys.front().time)return keys.front().value;
    const auto next=std::upper_bound(keys.begin(),keys.end(),seconds,[](float time,const auto& key){return time<key.time;});
    if(next==keys.end())return keys.back().value;
    const auto& first=*(next-1);const auto& last=*next;
    const float span=last.time-first.time;
    const float t=span>0?std::clamp((seconds-first.time)/span,0.0f,1.0f):1.0f,u=1.0f-t;
    std::array<float,3> result{};
    for(size_t axis=0;axis<3;++axis) {
        const float outgoing=first.outgoing[axis],incoming=last.incoming[axis];
        if(span<=0.032f||std::abs(outgoing)>100000.0f||std::abs(incoming)>100000.0f)
            result[axis]=seconds>first.time?last.value[axis]:first.value[axis];
        else if(std::abs(outgoing-first.value[axis])<0.0001f)
            result[axis]=u*first.value[axis]+t*last.value[axis];
        else
            result[axis]=u*u*u*first.value[axis]+3*u*u*t*outgoing+
                3*u*t*t*incoming+t*t*t*last.value[axis];
    }
    return result;
}

StorylandCutsceneCamera StorylandCutscene::cameraAt(float seconds) const {
    StorylandCutsceneCamera camera;
    const auto eye=sampleSpline(2,seconds),target=sampleSpline(3,seconds);
    camera.eye={offset.x+eye[0],offset.y+eye[1],offset.z+eye[2]};
    camera.target={offset.x+target[0],offset.y+target[1],offset.z+target[2]};
    camera.fieldOfView=std::clamp(sampleSpline(0,seconds)[0],1.0f,179.0f);
    camera.roll=sampleSpline(1,seconds)[0];return camera;
}

const StorylandCutscene::Subtitle* StorylandCutscene::subtitleAt(uint32_t milliseconds) const {
    for(const auto& subtitle:subtitleRows)if(milliseconds>=subtitle.start&&milliseconds<subtitle.end)return &subtitle;
    return nullptr;
}

std::string StorylandCutscene::audioStreamName() const {
    std::string result;
    for(const auto& subtitle:subtitleRows) {
        if(subtitle.audioStream.empty())continue;
        if(result.empty())result=subtitle.audioStream;
        else if(cutsceneLower(result)!=cutsceneLower(subtitle.audioStream))return {};
    }
    return result;
}

bool StorylandCutscene::load(const StorylandMocapArchive& mocap,const StorylandDtzArchive& archive,
    const std::string& name,std::string& error) {
    clear();
    StorylandCutscene candidate;
    candidate.nativeLcs=archive.gameVariant()==StorylandDtzGameVariant::Lcs;
    candidate.nativeLcsPs2=candidate.nativeLcs&&!archive.isPsp();
    // Both PS2 SetupCutsceneToStart implementations lift the actor origin by
    // one metre: LCS 0x3583D4 and VCS 0x428234. Their camera origin is set
    // before the lift, so it must remain at the authored CUT offset. Preserve
    // the existing PSP behavior until the PSP setup code has been verified.
    candidate.actorVerticalOffset=storylandCutsceneActorOriginLift(archive.gameVariant(),archive.isPsp());
    std::vector<uint8_t> definition,camera,animationBytes;
    const bool nativeScene=mocap.contains(name+".CUT")&&
        mocap.contains(name+".CAM")&&mocap.contains(name+".ANIM");
    const bool classicScene=mocap.contains(name+".CUT")&&
        mocap.contains(name+".DAT")&&mocap.contains(name+".IFP");
    const bool useNative=nativeScene&&
        (archive.gameVariant()==StorylandDtzGameVariant::Vcs||!classicScene);
    if(!nativeScene&&!classicScene) {
        error="Native cutscene '"+name+"' has no complete CUT/CAM/ANIM or CUT/DAT/IFP resource set.";
        return false;
    }
    const std::string cameraExtension=useNative?".CAM":".DAT";
    const std::string animationExtension=useNative?".ANIM":".IFP";
    if(!mocap.extract(name+".CUT",definition,error)||
        !mocap.extract(name+cameraExtension,camera,error)||
        !mocap.extract(name+animationExtension,animationBytes,error))return false;
    std::vector<std::string> models,clips;std::vector<bool> rootMotion;
    if(!candidate.loadDefinition(definition,models,clips,rootMotion,
        archive.gameVariant()==StorylandDtzGameVariant::Lcs,error)||!candidate.loadCamera(camera,error))return false;
    std::vector<StorylandPlayTestAsset> assets;
    if(!storylandLoadPlayTestModels(archive,models,assets,error))return false;
    StorylandAnimFile bank;
    const std::wstring animationPath=std::wstring(name.begin(),name.end())+
        std::wstring(animationExtension.begin(),animationExtension.end());
    const bool loadedBank=candidate.nativeLcs&&!useNative
        ? bank.loadLcsCutsceneIfp(animationBytes,animationPath,error)
        : bank.loadFromMemory(animationBytes,animationPath,error);
    if(!loadedBank||bank.clips().empty())return false;
    for(size_t index=0;index<models.size();++index) {
        const auto clip=std::find_if(bank.clips().begin(),bank.clips().end(),[&](const auto& value) {
            return cutsceneLower(value.name)==cutsceneLower(clips[index]);
        });
        if(clip==bank.clips().end()) {error="Cutscene actor has no matching animation clip: "+clips[index];return false;}
        Actor actor;actor.asset=std::move(assets[index]);actor.animation=bank;actor.clipName=clip->name;
        actor.rootMotion=rootMotion[index];
        if(!actor.animation.setActiveClipIndex(clip->index)||!actor.animation.hasDecodedMotion()) {
            error="Cutscene actor animation did not decode: "+clips[index];return false;
        }
        actor.posed=actor.asset.model.previewPoints();candidate.actorRows.push_back(std::move(actor));
    }
    candidate.loaded=true;
    if(!candidate.sample(0,error))return false;
    *this=std::move(candidate);error.clear();return true;
}

bool StorylandCutscene::sample(float seconds,std::string& error) {
    if(!loaded||!std::isfinite(seconds)) {error="Cutscene is not loaded or its sample time is invalid.";return false;}
    for(auto& actor:actorRows) {
        std::vector<StorylandModelPoint> bones;
        const float time=std::clamp(seconds,0.0f,std::max(0.0f,actor.animation.durationSeconds()-.001f));
        const StorylandAnimFile staticAnimation;
        if(!storylandSampleCutsceneModelAnimation(actor.asset.model,actor.staticBindPose?staticAnimation:actor.animation,time,
            actor.asset.model.previewPoints(),actor.posed,bones,error,&actor.frames,actor.staticBindPose,nativeLcsPs2)) {
            // An LCS CUT can reference a prop such as cssuitcase.mdl whose
            // authored clip controls a cutscene node but not a skinned mesh
            // bone. A rigid prop must remain visible without stopping SCM.
            const auto& skin=actor.asset.model.previewSkinWeights();
            const bool skinned=std::any_of(skin.begin(),skin.end(),[](const auto& vertex) {
                return vertex.valid&&vertex.influenceCount>0;
            });
            if(storylandCutsceneRigidPropMayHoldBindPose(error,skinned,
                actor.asset.model.armatureBones().size(),actor.asset.name)) {
                // Tony's LCS suitcase is animated through the CUT node,
                // not a humanoid skeleton. Sample that authored node
                // rather than freezing the suitcase at its bind pose.
                const auto* track=storylandFindRigidPropTrack(
                    actor.animation.tracks(),actor.asset.name);
                if(!track||!storylandSampleAuthoredRigidCutsceneProp(*track,time,
                    actor.asset.model.previewPoints(),actor.posed,actor.rootMotion)) {
                    // Rigid static CUT props without a root track
                    // still render, but never fake animated motion.
                    actor.posed=actor.asset.model.previewPoints();
                }
                error.clear();
            } else {
                error=actor.asset.name+": "+error;return false;
            }
        }
        for(auto& point:actor.posed)point.z+=actorVerticalOffset;
        actor.entityMatrix={1,0,0,0,0,1,0,0,0,0,1,0,0,0,actorVerticalOffset,1};
        if(!actor.staticBindPose&&!actor.animation.tracks().empty()) {
            const auto& root=actor.animation.tracks().front();
            if((root.channelFlags&2u)&&!root.keys.empty()) {
                const auto key=storylandSampleRigidTrack(root,time);
                actor.entityMatrix[12]=key.tx;actor.entityMatrix[13]=key.ty;actor.entityMatrix[14]+=key.tz;
            }
        }
        for(auto& frame:actor.frames)frame.matrix[14]+=actorVerticalOffset;
    }
    error.clear();return true;
}

bool StorylandCutscene::synchronizeScriptActors(const StorylandDtzArchive& archive,
    const StorylandMissionEnvironment& world,std::string& error) {
    if(!nativeLcs)return true;
    if(!loaded||actorRows.empty()) {error="Scripted cutscene actors require a loaded original animation bank.";return false;}
    actorRows.erase(std::remove_if(actorRows.begin(),actorRows.end(),[&](const auto& actor) {
        return actor.scriptHandle>=0&&std::none_of(world.cutscene.actors.begin(),world.cutscene.actors.end(),
            [&](const auto& declaration){return declaration.handle==actor.scriptHandle;});
    }),actorRows.end());
    for(const auto& declaration:world.cutscene.actors) {
        auto found=std::find_if(actorRows.begin(),actorRows.end(),[&](const auto& actor) {
            return actor.scriptHandle==declaration.handle;
        });
        if(found==actorRows.end()) {
            if(actorRows.size()>=128) {error="The native cutscene actor pool is full.";return false;}
            const int32_t model=declaration.model<0&&world.resolveObjectModel?
                world.resolveObjectModel(declaration.model):declaration.model;
            std::string name;size_t entry=0;
            const auto special=world.specialModelNames.find(model);
            if(special!=world.specialModelNames.end())name=special->second;
            else if(model<0||!archive.findModelDirEntryByModelId(uint32_t(model),entry,name)) {
                error="Scripted cutscene model has no original archive resource: "+std::to_string(declaration.model);
                return false;
            }
            std::vector<StorylandPlayTestAsset> assets;
            if(!storylandLoadPlayTestModels(archive,{name},assets,error))return false;
            Actor actor;actor.asset=std::move(assets.front());actor.animation=actorRows.front().animation;
            actor.scriptHandle=declaration.handle;actor.staticBindPose=true;
            actorRows.push_back(std::move(actor));found=actorRows.end()-1;
        }
        if(declaration.animation.empty()) {
            found->staticBindPose=true;found->clipName.clear();continue;
        }
        if(!found->staticBindPose&&cutsceneLower(found->clipName)==cutsceneLower(declaration.animation))continue;
        const auto clip=std::find_if(found->animation.clips().begin(),found->animation.clips().end(),
            [&](const auto& item){return cutsceneLower(item.name)==cutsceneLower(declaration.animation);});
        if(clip==found->animation.clips().end()||!found->animation.setActiveClipIndex(clip->index)||
            !found->animation.hasDecodedMotion()) {
            error="Scripted cutscene actor has no original animation clip: "+declaration.animation;return false;
        }
        found->clipName=clip->name;found->staticBindPose=false;
    }
    error.clear();return true;
}

namespace {
std::array<float,16> cutsceneMultiply(const std::array<float,16>& first,const std::array<float,16>& second) {
    std::array<float,16> result{};
    for(size_t column=0;column<4;++column)for(size_t row=0;row<4;++row)
        for(size_t axis=0;axis<4;++axis)result[column*4+row]+=first[axis*4+row]*second[column*4+axis];
    return result;
}
void cutsceneTransform(StorylandModelPoint& point,const std::array<float,16>& matrix,
    const std::array<float,3>& offset) {
    const float x=point.x,y=point.y,z=point.z;
    point.x=matrix[0]*x+matrix[4]*y+matrix[8]*z+matrix[12]-offset[0];
    point.y=matrix[1]*x+matrix[5]*y+matrix[9]*z+matrix[13]-offset[1];
    point.z=matrix[2]*x+matrix[6]*y+matrix[10]*z+matrix[14]-offset[2];
    storylandTransformPs2ModelNormal(point,point,matrix.data());
    if(point.hasNormal) {
        const float nx=point.nx,ny=point.ny,nz=point.nz;
        const float determinant=matrix[0]*(matrix[5]*matrix[10]-matrix[9]*matrix[6])-
            matrix[4]*(matrix[1]*matrix[10]-matrix[9]*matrix[2])+
            matrix[8]*(matrix[1]*matrix[6]-matrix[5]*matrix[2]);
        if(!std::isfinite(determinant)||std::abs(determinant)<0.0000001f) {point.hasNormal=false;return;}
        point.nx=((matrix[5]*matrix[10]-matrix[9]*matrix[6])*nx+
            (matrix[9]*matrix[2]-matrix[1]*matrix[10])*ny+(matrix[1]*matrix[6]-matrix[5]*matrix[2])*nz)/determinant;
        point.ny=((matrix[8]*matrix[6]-matrix[4]*matrix[10])*nx+
            (matrix[0]*matrix[10]-matrix[8]*matrix[2])*ny+(matrix[4]*matrix[2]-matrix[0]*matrix[6])*nz)/determinant;
        point.nz=((matrix[4]*matrix[9]-matrix[8]*matrix[5])*nx+
            (matrix[8]*matrix[1]-matrix[0]*matrix[9])*ny+(matrix[0]*matrix[5]-matrix[4]*matrix[1])*nz)/determinant;
        const float length=std::hypot(point.nx,point.ny,point.nz);
        if(std::isfinite(length)&&length>0.000001f) {point.nx/=length;point.ny/=length;point.nz/=length;}
        else point.hasNormal=false;
    }
}
}

bool StorylandCutscene::actorAttachmentMatrix(int32_t handle,int32_t bone,const std::string& component,
    std::array<float,16>& matrix) const {
    const auto actor=std::find_if(actorRows.begin(),actorRows.end(),
        [&](const auto& item){return item.scriptHandle==handle;});
    if(actor==actorRows.end())return false;
    if(bone<0&&component.empty()) {matrix=actor->entityMatrix;return true;}
    const auto frame=std::find_if(actor->frames.begin(),actor->frames.end(),[&](const auto& item) {
        return bone>=0?item.boneId==uint32_t(bone):cutsceneLower(item.name)==cutsceneLower(component);
    });
    if(frame==actor->frames.end())return false;
    matrix=frame->matrix;return true;
}

bool StorylandCutscene::applyAttachments(const StorylandMissionEnvironment& world,std::string& error) {
    if(!nativeLcs)return true;
    const std::array<float,3> offset{world.cutscene.offsetX,world.cutscene.offsetY,world.cutscene.offsetZ};
    const std::array<float,16> sceneMatrix{1,0,0,0,0,1,0,0,0,0,1,0,offset[0],offset[1],offset[2],1};
    std::vector<uint8_t> visited(actorRows.size(),0);
    std::function<bool(size_t)> apply=[&](size_t index) {
        if(visited[index]==2)return true;
        if(visited[index]==1) {error="Scripted cutscene attachments contain a parent cycle.";return false;}
        visited[index]=1;
        auto& actor=actorRows[index];
        auto transform=sceneMatrix;
        const auto found=world.lcsExtendedWorld.cutsceneAttachments.find(actor.scriptHandle);
        const bool attached=actor.scriptHandle>=0&&found!=world.lcsExtendedWorld.cutsceneAttachments.end();
        if(attached) {
            const auto& attachment=found->second;
            bool resolved=false;
            const auto parent=std::find_if(actorRows.begin(),actorRows.end(),
                [&](const auto& item){return item.scriptHandle==attachment.parent;});
            if(attachment.vehicle)resolved=world.vehicleWorldMatrix&&world.vehicleWorldMatrix(attachment.parent,transform);
            else if(parent!=actorRows.end()) {
                if(!apply(size_t(parent-actorRows.begin())))return false;
                resolved=actorAttachmentMatrix(attachment.parent,attachment.bone,attachment.component,transform);
            } else resolved=world.lcsExtendedWorld.objectAttachmentMatrix&&
                world.lcsExtendedWorld.objectAttachmentMatrix(attachment.parent,attachment.bone,attachment.component,transform);
            if(!resolved||!std::all_of(transform.begin(),transform.end(),[](float value){return std::isfinite(value);})) {
                error="A scripted cutscene attachment has no valid native parent frame.";return false;
            }
        }
        actor.entityMatrix=attached?transform:cutsceneMultiply(sceneMatrix,actor.entityMatrix);
        for(auto& frame:actor.frames)frame.matrix=cutsceneMultiply(transform,frame.matrix);
        if(attached)for(auto& point:actor.posed)cutsceneTransform(point,transform,offset);
        visited[index]=2;return true;
    };
    for(size_t index=0;index<actorRows.size();++index)if(!apply(index))return false;
    error.clear();return true;
}


void StorylandCutscenePlayer::clear() {
    loadedScene.clear();loadSeen=startSeen=clearSeen=0;
    completionFadeStarted=false;
    skipTransition={};skipCameraValid=false;
}

void StorylandCutscenePlayer::discard(StorylandMissionEnvironment& world) {
    clear();
    auto& state=world.cutscene;
    state.playing=state.loaded=state.finished=false;
    state.actors.clear();
    state.subtitleKey.clear();state.subtitleText.clear();
    ++state.clearRequest;
    loadSeen=state.loadRequest;
    startSeen=state.startRequest;
    clearSeen=state.clearRequest;
}

bool StorylandCutscenePlayer::advanceSkip(StorylandMissionEnvironment& world) {
    if(!skipTransition.active)return false;
    auto& state=world.cutscene;
    // Native VCS 0x428980 starts a 1000ms timer and 0x428998 requests
    // a one-second camera fade. FinishCutscene runs only when the timer is
    // negative. Keep cinematic ownership through that last black frame.
    if(skipCameraValid&&!loadedScene.ready()) {
        auto& camera=world.camera;
        camera.x=skipCamera.eye.x;camera.y=skipCamera.eye.y;camera.z=skipCamera.eye.z;
        camera.targetX=skipCamera.target.x;camera.targetY=skipCamera.target.y;camera.targetZ=skipCamera.target.z;
        camera.upX=skipCameraUp[0];camera.upY=skipCameraUp[1];camera.upZ=skipCameraUp[2];
        camera.fieldOfView=skipCamera.fieldOfView;camera.roll=skipCamera.roll;
        camera.active=camera.positionSet=camera.targetSet=camera.cutsceneOwned=true;
        camera.restoreRequested=false;camera.transitioning=false;
    }
    const float alpha=skipTransition.alphaAt(world.gameMilliseconds);
    world.fadeAlpha=std::max(world.fadeAlpha,alpha);
    world.fadeTargetAlpha=1;world.fadeDirection=0;
    if(alpha<1||(skipTransition.durationMilliseconds&&
        uint32_t(world.gameMilliseconds-skipTransition.startedAt)<=skipTransition.durationMilliseconds))return true;
    skipTransition.active=false;
    world.fadeAlpha=1;world.fadeDuration=0;world.fading=false;
    if(loadedScene.ready())state.elapsedMilliseconds=uint32_t(std::ceil(loadedScene.durationSeconds()*1000));
    state.playing=false;state.finished=true;
    state.subtitleKey.clear();state.subtitleText.clear();
    world.restorePlayerAfterCutscene();
    return true;
}

bool StorylandCutscenePlayer::update(uint32_t elapsedMilliseconds,const StorylandMocapArchive& mocap,
    const StorylandDtzArchive& archive,StorylandMissionEnvironment& world,std::string& error,bool sampleActors) {
    auto& state=world.cutscene;
    if(skipTransition.active&&!loadedScene.ready()) {advanceSkip(world);error.clear();return true;}
    if(skipTransition.active) {state.playing=true;state.finished=false;}
    if(!skipTransition.active&&state.clearRequest!=clearSeen) {
        clearSeen=state.clearRequest;loadedScene.clear();
        completionFadeStarted=false;
        state.subtitleKey.clear();state.subtitleText.clear();
        if(world.camera.cutsceneOwned) {
            world.camera.active=false;world.camera.restoreRequested=true;
            world.camera.fieldOfView=70;world.camera.roll=0;world.camera.cutsceneOwned=false;
        }
    }
    if(!skipTransition.active&&state.loadRequest!=loadSeen) {
        loadSeen=state.loadRequest;state.loaded=false;
        completionFadeStarted=false;
        world.cutsceneSkipped=false;
        if(!loadedScene.load(mocap,archive,state.name,error))return false;
        if(!state.offsetSet) {
            state.offsetX=loadedScene.origin().x;state.offsetY=loadedScene.origin().y;state.offsetZ=loadedScene.origin().z;
            state.offsetSet=true;
        }
        state.loaded=true;
    }
    if(!skipTransition.active&&state.startRequest!=startSeen) {
        startSeen=state.startRequest;
        if(!loadedScene.ready()||!state.loaded) {error="Requested cutscene has no decoded actor and camera resources.";return false;}
        completionFadeStarted=false;
        state.elapsedMilliseconds=0;state.finished=false;state.playing=true;
    }
    if(loadedScene.ready()&&!loadedScene.synchronizeScriptActors(archive,world,error))return false;
    if(!state.playing||!loadedScene.ready()) {
        state.subtitleKey.clear();state.subtitleText.clear();
        return true;
    }
    const uint32_t duration=uint32_t(std::ceil(loadedScene.durationSeconds()*1000.0f));
    state.elapsedMilliseconds=uint32_t(std::min(uint64_t(duration),uint64_t(state.elapsedMilliseconds)+elapsedMilliseconds));
    const float seconds=float(state.elapsedMilliseconds)/1000.0f;
    if(sampleActors&&(!loadedScene.sample(seconds,error)||!loadedScene.applyAttachments(world,error)))return false;
    const auto camera=loadedScene.cameraAt(seconds);
    auto& target=world.camera;
    const float dx=state.offsetX-loadedScene.origin().x,dy=state.offsetY-loadedScene.origin().y,dz=state.offsetZ-loadedScene.origin().z;
    target.x=camera.eye.x+dx;target.y=camera.eye.y+dy;target.z=camera.eye.z+dz;
    target.targetX=camera.target.x+dx;target.targetY=camera.target.y+dy;target.targetZ=camera.target.z+dz;
    target.active=target.positionSet=target.targetSet=true;target.cutsceneOwned=true;target.restoreRequested=false;
    target.upX=target.upY=target.upZ=0;target.fieldOfView=camera.fieldOfView;target.roll=camera.roll;
    const auto* subtitle=loadedScene.subtitleAt(state.elapsedMilliseconds);
    state.subtitleKey=subtitle?subtitle->key:std::string{};
    state.subtitleText=subtitle?subtitle->text:std::string{};
    // Native 0x4285EC advances cinematic time before checking the skip timer.
    // Continue the authored camera and actor motion throughout the outward fade.
    if(skipTransition.active) {advanceSkip(world);error.clear();return true;}
    if(!state.loop&&!completionFadeStarted) {
        const auto name=cutsceneLower(state.name);
        const bool vcs=archive.gameVariant()==StorylandDtzGameVariant::Vcs;
        const uint32_t lead=vcs&&name=="renic4"?1400u:vcs&&name=="mara2"?5000u:1000u;
        // VCS 0x4288C4 and LCS 0x358970 request a one-second camera
        // fade before natural completion, independently of SCM fade commands.
        if(name!="finale"&&uint64_t(state.elapsedMilliseconds)+lead>duration) {
            completionFadeStarted=true;
            world.fadeStartAlpha=world.fadeAlpha=std::clamp(world.fadeAlpha,0.0f,1.0f);
            world.fadeTargetAlpha=1;world.fadeDirection=0;
            world.fadeTotal=world.fadeDuration=1000;
            world.fading=true;
        }
    }
    if(state.elapsedMilliseconds>=duration&&!state.loop) {
        if(completionFadeStarted) {
            // The host advances script fades before sampling the cutscene.
            // Complete that final camera frame before SCM can relocate Victor,
            // including frames that cross the entire ending fade interval.
            world.fadeAlpha=world.fadeStartAlpha=world.fadeTargetAlpha=1;
            world.fadeDuration=0;world.fading=false;
        }
        state.playing=false;state.finished=true;
        state.subtitleKey.clear();state.subtitleText.clear();
    }
    else if(state.elapsedMilliseconds>=duration)state.elapsedMilliseconds=0;
    error.clear();return true;
}

bool StorylandCutscenePlayer::actorAttachmentMatrix(const StorylandDtzArchive& archive,
    StorylandMissionEnvironment& world,int32_t handle,int32_t bone,const std::string& component,
    std::array<float,16>& matrix) {
    if(!loadedScene.ready()||std::none_of(world.cutscene.actors.begin(),world.cutscene.actors.end(),
        [&](const auto& actor){return actor.handle==handle;}))return false;
    std::string error;
    if(!loadedScene.synchronizeScriptActors(archive,world,error)||
        !loadedScene.sample(float(world.cutscene.elapsedMilliseconds)*0.001f,error)||
        !loadedScene.applyAttachments(world,error))return false;
    return loadedScene.actorAttachmentMatrix(handle,bone,component,matrix);
}

void StorylandCutscenePlayer::skip(StorylandMissionEnvironment& world,uint32_t fadeMilliseconds) {
    auto& state=world.cutscene;
    if(skipTransition.active||!state.playing)return;
    world.cutsceneSkipped=true;
    skipTransition.begin(world.gameMilliseconds,world.fadeAlpha,fadeMilliseconds);
    skipCamera={};skipCameraValid=world.camera.active&&world.camera.positionSet&&world.camera.targetSet;
    if(skipCameraValid) {
        skipCamera.eye={world.camera.x,world.camera.y,world.camera.z};
        skipCamera.target={world.camera.targetX,world.camera.targetY,world.camera.targetZ};
        skipCamera.fieldOfView=world.camera.fieldOfView;skipCamera.roll=world.camera.roll;
        skipCameraUp={world.camera.upX,world.camera.upY,world.camera.upZ};
    }
    world.fadeAlpha=world.fadeStartAlpha=skipTransition.initialAlpha;world.fadeTargetAlpha=1;
    world.fadeTotal=world.fadeDuration=float(fadeMilliseconds);world.fadeDirection=0;
    world.fading=fadeMilliseconds>0;world.fadingColor={0,0,0};
    if(!fadeMilliseconds)world.fadeAlpha=1;
    // Do not seek to the final actor/camera frame or restore control here.
    // Updates keep the cinematic timeline alive and publish completion at black.
    state.finished=false;
    state.subtitleKey.clear();state.subtitleText.clear();
}
