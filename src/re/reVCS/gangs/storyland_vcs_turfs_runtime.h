#pragma once

#include "src/re/reVCS/gangs/storyland_vcs_gang_turfs.h"
#include "src/storyland/storyland_atomic_io.h"
#include "src/storyland/storyland_inverted_map.h"
#include "src/re/reVCS/modloader/storyland_vcs_modloader.h"
#include "src/re/reVCS/services/storyland_army_tolerance.h"
#include "src/re/reVCS/gangs/storyland_vcs_turf_unlock.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <map>
#include <string>
#include <sstream>
#include <vector>

class StorylandVcsTurfRuntime final : public storyland::VcsTurfHost {
public:
    static bool gameplayUnlocked() {
        const auto& game=gPlayTestMissionEnvironment;
        return gPlayTestActive&&storylandVcsTurfUnlocked(gPlayTestMissionVm.vcs,
            gPlayTestRuntime.extendedAndSaFeatures,
            game.passedMissions,game.empireSites);
    }
    void reset() {
        gPlayTestRuntime.extendedGangSlotAt={};
        if(initialized_) {save();turfs_.resetCombat(*this);}
        retireHaitianPopulation();
        gPlayTestRuntime.extendedHaitianGang={};
        turfs_=storyland::VcsGangTurfs{};
        spawned_.clear();
        lifeGenerations_.clear();
        previouslyAlive_.clear();
        initialized_=false;
        lastElapsed_=-1.0f;
        lastDayMinutes_=-1.0;
        totalGameMinutes_=0;
        message_.clear();
        error_.clear();
        haitianModelsAttempted_=false;
        haitianAssetError_.clear();
    }

    void tick() {
        if(!gameplayUnlocked()||!hasDataMod()) {
            gPlayTestRuntime.extendedGangSlotAt={};
            if(initialized_) {
                if(turfs_.enabled())save();
                storyland::VcsTurfFrame disabled;
                disabled.realTimeSeconds=gPlayTestRuntime.elapsed;
                disabled.elapsedGameMinutes=totalGameMinutes_;
                turfs_.update(disabled,*this);
            }
            retireHaitianPopulation();
            gPlayTestRuntime.extendedHaitianGang={};
            haitianModelsAttempted_=false;
            lastDayMinutes_=std::clamp(double(gPlayTestRuntime.clockHour),0.0,24.0)*60.0;
            return;
        }
        auto& runtime=gPlayTestRuntime;
        runtime.extendedGangSlotAt=[this](const StorylandNativePoint& point,int32_t slot) {
            return populationGangAt(point,slot);
        };
        const auto& world=gDtzArchive.nativeWorld();
        if(lastElapsed_>runtime.elapsed+0.1f)reset();
        lastElapsed_=runtime.elapsed;
        if(!haitianModelsAttempted_) {
            haitianModelsAttempted_=true;
            loadHaitianModels();
        }
        if(!initialized_) {
            if(!initialize(world))return;
        }
        const auto gameClock=std::clamp(double(runtime.clockHour),0.0,24.0)*60.0;
        if(lastDayMinutes_>=0.0) {
            double elapsed=gameClock-lastDayMinutes_;
            if(elapsed < -720.0)elapsed+=1440.0;
            if(elapsed>0.0)totalGameMinutes_+=std::min(elapsed,1440.0);
        }
        lastDayMinutes_=gameClock;
        storyland::VcsTurfFrame frame;
        frame.playerPosition={gPlayerWorldX,gPlayerWorldY};
        frame.playerAlive=runtime.health>0 && !runtime.busted;
        frame.playerOnFoot=runtime.occupiedVehicle<0 &&
            runtime.vehicleAction==StorylandVehicleAction::None;
        frame.scriptedMissionActive=gPlayTestMissionEnvironment.introMissionProtected ||
            gPlayTestMissionEnvironment.cutscene.playing ||
            gPlayTestMissionProgram.activeMission()>=0;
        frame.gameIsVcs=true;
        frame.extendedAndSaFeatures=true;
        frame.paused=runtime.paused||runtime.pauseMenu||gPlayTestDebugMenuOpen;
        frame.playerInExterior=runtime.visibleArea==0&&gPlayTestMissionEnvironment.area==0;
        frame.realTimeSeconds=runtime.elapsed;
        frame.elapsedGameMinutes=totalGameMinutes_;
        turfs_.update(frame,*this);
        if(!frame.paused)registerDeaths();
    }

    void drawHud() const {
        if(!gameplayUnlocked()||!initialized_||!turfs_.enabled()||!gPlayTestRuntime.hud||gPlayTestRuntime.pauseMenu||
            gPlayTestRuntime.paused||gPlayTestMissionEnvironment.cutscene.playing)return;
        auto* draw=ImGui::GetForegroundDrawList();
        const auto size=ImGui::GetIO().DisplaySize;
        const auto player=storyland::VcsTurfPoint{gPlayerWorldX,gPlayerWorldY};
        const auto* current=turfs_.turfAt(player);
        if(current) {
            const ImU32 tint=current->owner==storyland::VcsTurfGang::Cholos?
                IM_COL32(255,244,170,255):
                current->owner==storyland::VcsTurfGang::Haitians?
                IM_COL32(129,141,229,255):IM_COL32(79,225,111,255);
            const std::string text=current->displayName+" | "+
                (current->owner==storyland::VcsTurfGang::Cholos?"Cholos":
                current->owner==storyland::VcsTurfGang::Haitians?"Haitians":"Vance Gang");
            draw->AddText(ImVec2(24.0f,size.y-207.0f),tint,text.c_str());
        }
        const auto battle=turfs_.battleStatus();
        if(turfs_.inCaptureWar()) {
            std::string label;
            if(battle.defending) {
                const auto* attacked=turfs_.turfById(battle.turfId);
                label=(battle.awaitingPlayer?"Territory under attack: ":"Defend: ")+
                    (attacked?attacked->displayName:std::string("Mainland turf"))+"  |  "+
                    std::to_string(int(std::ceil(battle.secondsRemaining)))+"s";
                if(!battle.awaitingPlayer)label+="  |  Enemies "+std::to_string(battle.enemyCount);
            } else if(battle.waitingForWave) {
                label="Prepare for wave "+std::to_string(battle.waveNumber)+"/"+
                    std::to_string(battle.totalWaves)+"  |  "+
                    std::to_string(int(std::ceil(battle.secondsRemaining)))+"s";
            } else {
                label="Gang war  |  Wave "+std::to_string(battle.waveNumber)+
                    "/"+std::to_string(battle.totalWaves)+"  |  Enemies "+std::to_string(battle.enemyCount);
            }
            draw->AddText(ImVec2(size.x*.5f-ImGui::CalcTextSize(label.c_str()).x*.5f,
                size.y*.14f),IM_COL32(245,205,120,255),label.c_str());
        }
        if(!error_.empty())draw->AddText(ImVec2(24.0f,size.y-184.0f),
            IM_COL32(245,177,90,255),error_.c_str());
        if(!message_.empty()&&gPlayTestRuntime.elapsed<messageUntil_) {
            draw->AddText(ImVec2(size.x*.5f-ImGui::CalcTextSize(message_.c_str()).x*.5f,
                size.y*.19f),IM_COL32(250,220,170,255),message_.c_str());
        }
    }

    void drawRadar(ImDrawList* draw,ImVec2 center,ImVec2 radius,
        const StorylandNativePoint& origin,float cameraYaw,float range,bool invertedMap) const {
        if(!gameplayUnlocked()||!initialized_||!turfs_.enabled()||!gPlayTestRuntime.hud||
            gPlayTestRuntime.pauseMenu||gPlayTestMissionEnvironment.cutscene.playing||
            !draw||radius.x<=0||radius.y<=0||!std::isfinite(range)||range<=0)return;
        const float angle=cameraYaw*0.01745329251994329577f;
        const auto project=[&](float x,float y) {
            const float dx=x-origin.x,dy=y-origin.y;
            return StorylandRadarVertex{
                storylandInvertedMapHorizontal((std::sin(angle)*dx-std::cos(angle)*dy)/range,invertedMap),
                -(std::cos(angle)*dx+std::sin(angle)*dy)/range,0,0};
        };
        const auto battle=turfs_.battleStatus();
        for(const auto& turf:turfs_.turfs()) {
            const auto& rect=turf.bounds;
            const std::array<StorylandRadarVertex,4> corners{
                project(rect.minX,rect.minY),project(rect.maxX,rect.minY),
                project(rect.maxX,rect.maxY),project(rect.minX,rect.maxY)};
            const auto clipped=storylandClipRadarTile(corners,0,0,1);
            if(clipped.size()<3)continue;
            std::vector<ImVec2> vertices;
            vertices.reserve(clipped.size());
            for(const auto& v:clipped)vertices.emplace_back(center.x+v.x*radius.x,center.y+v.y*radius.y);
            ImU32 tint=turf.owner==storyland::VcsTurfGang::Cholos?
                IM_COL32(255,242,151,72):
                turf.owner==storyland::VcsTurfGang::Haitians?
                IM_COL32(51,59,132,72):IM_COL32(53,177,74,72);
            if(!battle.turfId.empty()&&turf.id==battle.turfId&&
                (int(gPlayTestRuntime.elapsed*2.0f)&1))tint=IM_COL32(235,35,35,110);
            draw->AddConvexPolyFilled(vertices.data(),int(vertices.size()),tint);
            if(!battle.turfId.empty()&&turf.id==battle.turfId)
                draw->AddPolyline(vertices.data(),int(vertices.size()),
                    IM_COL32(255,244,140,220),ImDrawFlags_Closed,2.0f);
        }
        if(battle.defending&&battle.awaitingPlayer) {
            const auto* attacked=turfs_.turfById(battle.turfId);
            if(attacked) {
                auto marker=project((attacked->bounds.minX+attacked->bounds.maxX)*.5f,
                    (attacked->bounds.minY+attacked->bounds.maxY)*.5f);
                const float distance=std::hypot(marker.x,marker.y);
                if(distance>.86f) {marker.x*=.86f/distance;marker.y*=.86f/distance;}
                draw->AddCircleFilled(ImVec2(center.x+marker.x*radius.x,center.y+marker.y*radius.y),
                    std::max(2.0f,std::min(radius.x,radius.y)*.05f),IM_COL32(245,40,40,240),12);
            }
        }
    }

    const std::string& error() const {return error_;}
    const storyland::VcsGangTurfs& gameplay() const {return turfs_;}
    bool hasDataMod() const {return gangTurfConfig()!=nullptr;}

    bool bindHaitianModels(std::string& error) {
        auto& runtime=gPlayTestRuntime;
        const auto* config=gangTurfConfig();
        if(!config) {error="No enabled gang-turf data mod is mounted.";return false;}
        if(config->populationKind!=StorylandExtendedHaitianGang::populationKind||
            config->pedestrianType!=StorylandExtendedHaitianGang::pedestrianType||
            config->health<=0||config->health>10000||config->armour<0||config->armour>10000) {
            error="The supplemental gang data has unsupported identity or health values.";
            return false;
        }
        if(runtime.pedestrianModelStats.size()!=gPlayTestPedestrianAssets.size()||
            runtime.pedestrianModelRoles.size()!=gPlayTestPedestrianAssets.size()) {
            error="Haitian binding requires initialized pedestrian population assets.";
            return false;
        }
        const auto& names=config->haitianModels;
        std::array<uint32_t,2> identities{};
        for(size_t index=0;index<names.size();++index)
            if(!gDtzArchive.findModelIdByName(names[index],identities[index])) {
                error="Requested Haitian model is unavailable in the VCS archive: "+names[index];
                return false;
            }
        std::array<int32_t,2> weapons{-1,-1};
        for(const auto& weapon:runtime.weapons) {
            if(weapon.fireType!=1||weapon.nativeId<0)continue;
            if(weapon.slot==config->weaponSlots[0]&&weapons[0]<0)weapons[0]=weapon.nativeId;
            if(weapon.slot==config->weaponSlots[1]&&weapons[1]<0)weapons[1]=weapon.nativeId;
        }
        if(weapons[0]<0||weapons[1]<0) {
            error="The supplemental gang loadout requires native hitscan weapons in its configured classes.";
            return false;
        }
        std::array<size_t,2> slots{SIZE_MAX,SIZE_MAX};
        for(size_t model=0;model<gPlayTestPedestrianAssets.size();++model)
            for(size_t index=0;index<identities.size();++index)
                if(gPlayTestPedestrianAssets[model].pedStats.valid&&
                    gPlayTestPedestrianAssets[model].pedStats.modelId==identities[index])slots[index]=model;
        std::array<StorylandPlayTestAsset,2> decoded;
        for(size_t index=0;index<names.size();++index) {
            if(slots[index]!=SIZE_MAX)continue;
            std::vector<StorylandPlayTestAsset> candidates;
            if(!storylandLoadPlayTestModels(gDtzArchive,{names[index]},candidates,error)||candidates.size()!=1)
                return false;
            auto& asset=candidates.front();
            if(!asset.pedStats.valid||asset.pedStats.modelId!=identities[index]||
                asset.model.modelKind()!=StorylandModelKind::PedModel||
                asset.model.armatureBones().empty()||asset.images.empty()) {
                error="Requested Haitian model needs complete native geometry, skeleton and textures: "+names[index];
                return false;
            }
            decoded[index]=std::move(asset);
        }
        const size_t additions=size_t(slots[0]==SIZE_MAX)+size_t(slots[1]==SIZE_MAX);
        gPlayTestPedestrianAssets.reserve(gPlayTestPedestrianAssets.size()+additions);
        runtime.pedestrianModelStats.reserve(runtime.pedestrianModelStats.size()+additions);
        runtime.pedestrianModelRoles.reserve(runtime.pedestrianModelRoles.size()+additions);
        for(size_t index=0;index<identities.size();++index) {
            auto& asset=decoded[index];
            if(slots[index]==SIZE_MAX) {
                runtime.pedestrianModelStats.push_back(asset.pedStats);
                runtime.pedestrianModelRoles.push_back(StorylandServiceRole::Civilian);
                gPlayTestPedestrianAssets.push_back(std::move(asset));
            }
        }
        runtime.extendedHaitianGang=StorylandExtendedHaitianGang{};
        runtime.extendedHaitianGang.pedModels={int32_t(identities[0]),int32_t(identities[1])};
        runtime.extendedHaitianGang.weapons=weapons;
        runtime.extendedHaitianGang.health=config->health;
        runtime.extendedHaitianGang.armour=config->armour;
        runtime.extendedHaitianGang.available=true;
        haitianAssetError_.clear();
        error.clear();
        return true;
    }

    static std::filesystem::path gameSlotSidecar(std::filesystem::path slotPath) {
        slotPath+=".turfs";
        return slotPath;
    }
    bool captureGameSave(std::vector<uint8_t>& bytes,std::string& error) const {
        std::ostringstream output;
        if(initialized_) {
            if(!turfs_.writeSave(output)) {error="Cannot serialize gang turf ownership.";return false;}
        } else output<<"STORYLAND_VCS_TURFS_ABSENT 1\n";
        const auto text=output.str();
        bytes.assign(text.begin(),text.end());
        error.clear();
        return true;
    }
    bool prepareGameSlot(const std::filesystem::path& slotPath,
        storyland::VcsGangTurfs& restored,std::string& error) const {
        if(!hasDataMod()) {
            restored=storyland::VcsGangTurfs{};
            error.clear();
            return true;
        }
        const auto sidecar=gameSlotSidecar(slotPath);
        std::error_code status;
        const bool exists=std::filesystem::exists(sidecar,status);
        if(status) {error="Cannot inspect gang turf progress for this save.";return false;}
        std::string text;
        if(exists) {
            std::ifstream input(sidecar,std::ios::binary|std::ios::ate);
            if(!input) {error="Cannot open gang turf progress for this save.";return false;}
            const auto size=input.tellg();
            if(size<0||uint64_t(size)>65536) {error="Gang turf progress has an invalid size.";return false;}
            text.resize(size_t(size));input.seekg(0);
            if(!input.read(text.data(),std::streamsize(text.size()))) {
                error="Cannot read gang turf progress for this save.";return false;
            }
        }
        const bool haveProgress=exists&&text!="STORYLAND_VCS_TURFS_ABSENT 1\n";
        StorylandVcsTurfRuntime candidate;
        if(!candidate.initialize(gDtzArchive.nativeWorld(),false)) {
            if(haveProgress) {error=candidate.error_;return false;}
            restored=storyland::VcsGangTurfs{};
            error.clear();return true;
        }
        if(haveProgress) {
            std::istringstream progress(text);
            if(!candidate.turfs_.readSave(progress)) {error="Gang turf progress is invalid.";return false;}
            progress>>std::ws;
            if(progress.peek()!=std::char_traits<char>::eof()) {
                error="Gang turf progress contains unexpected trailing data.";return false;
            }
        }
        restored=std::move(candidate.turfs_);
        error.clear();return true;
    }
    void restoreGameSlot(storyland::VcsGangTurfs restored) {
        gPlayTestRuntime.extendedGangSlotAt={};
        if(initialized_)turfs_.resetCombat(*this);
        turfs_=std::move(restored);
        initialized_=!turfs_.turfs().empty();
        spawned_.clear();lifeGenerations_.assign(gPlayTestRuntime.pedestrians.size(),1);
        previouslyAlive_.assign(gPlayTestRuntime.pedestrians.size(),false);
        for(size_t i=0;i<gPlayTestRuntime.pedestrians.size();++i)
            previouslyAlive_[i]=gPlayTestRuntime.pedestrians[i].health>0;
        lastElapsed_=gPlayTestRuntime.elapsed;
        lastDayMinutes_=std::clamp(double(gPlayTestRuntime.clockHour),0.0,24.0)*60.0;
        totalGameMinutes_=0.0;
        message_.clear();
        error_=findGangSlot(storyland::VcsTurfGang::Haitians)<0?
            haitianWarning():std::string();
    }

    bool spawnGangWarEnemies(const storyland::VcsTurfBattle& battle,
        const storyland::VcsTurfRect& area,std::vector<std::uint64_t>& spawnedIds) override {
        spawnedIds.clear();
        if(!gNativePedestriansEnabled||battle.enemyCount<=0||battle.enemyCount>32)return false;
        const int32_t gangSlot=findGangSlot(battle.defender);
        if(gangSlot<0) {
            error_=battle.defender==storyland::VcsTurfGang::Haitians?
                haitianWarning():
                "The native gang models are not available.";
            return false;
        }
        StorylandMissionEnvironment::Gang gang;
        if(gangSlot==StorylandExtendedHaitianGang::populationKind) {
            const auto& customGang=gPlayTestRuntime.extendedHaitianGang;
            gang.pedModels=customGang.pedModels;gang.weapons=customGang.weapons;
            gang.health=customGang.health;gang.armour=customGang.armour;
        } else gang=gPlayTestMissionEnvironment.gangs[size_t(gangSlot)];
        const auto& world=gDtzArchive.nativeWorld();
        const auto& nodes=world.paths.nodes;
        if(nodes.empty())return false;
        struct SpawnPlan {
            size_t slot=0,model=0,weapon=0;
            uint32_t node=0;
            StorylandNativePoint point;
        };
        std::vector<SpawnPlan> plans;
        for(int i=0;i<battle.enemyCount;++i) {
            const size_t model=findGangModel(gangSlot,size_t(i+battle.waveNumber));
            if(model>=gPlayTestPedestrianAssets.size())return false;
            const int32_t weaponId=gang.weapons[size_t(i+battle.waveNumber)&1u];
            const auto weapon=std::find_if(gPlayTestRuntime.weapons.begin(),gPlayTestRuntime.weapons.end(),
                [&](const auto& value){return value.nativeId==weaponId;});
            if(weapon==gPlayTestRuntime.weapons.end())return false;
            std::vector<size_t> reserved;
            for(const auto& plan:plans)reserved.push_back(plan.slot);
            const size_t slot=availableSlot(reserved);
            if(slot>=gPlayTestRuntime.pedestrians.size())return false;
            bool placed=false;
            for(size_t attempt=0;attempt<nodes.size();++attempt) {
                const size_t nodeIndex=(size_t(i)*997u+attempt)%nodes.size();
                const auto& node=nodes[nodeIndex];
                if(node.vehicle||node.water||node.disabled||node.links.empty()||
                    !area.contains({node.position.x,node.position.y}))continue;
                const float distance=std::hypot(node.position.x-gPlayerWorldX,node.position.y-gPlayerWorldY);
                if(distance<19.0f||distance>85.0f)continue;
                bool occupied=false;
                for(size_t other=0;other<gNativePedestrians.size()&&
                    other<gPlayTestRuntime.pedestrians.size();++other) {
                    if(std::find(reserved.begin(),reserved.end(),other)!=reserved.end()||
                        gPlayTestRuntime.pedestrians[other].health<=0)continue;
                    const auto& point=gNativePedestrians[other].position;
                    if(std::hypot(point.x-node.position.x,point.y-node.position.y)<3.0f) {occupied=true;break;}
                }
                for(const auto& plan:plans)
                    if(std::hypot(plan.point.x-node.position.x,plan.point.y-node.position.y)<3.0f)
                        occupied=true;
                if(occupied)continue;
                uint8_t surface=0;
                const StorylandNativePoint high{node.position.x,node.position.y,node.position.z+1.0f};
                const StorylandNativePoint low{node.position.x,node.position.y,node.position.z-2.0f};
                const float fraction=storylandTraceNativeGround(world,high,low,surface);
                if(fraction>=1.0f)continue;
                const StorylandNativePoint point{high.x,high.y,high.z+(low.z-high.z)*fraction};
                if(!storylandNativeBodyClear(world,point,storylandNativeCharacterBody()))continue;
                plans.push_back({slot,model,size_t(weapon-gPlayTestRuntime.weapons.begin()),
                    uint32_t(nodeIndex),point});
                placed=true;
                break;
            }
            if(!placed)return false;
        }
        error_.clear();
        for(const auto& plan:plans) {
            while(gNativePedestrians.size()<=plan.slot)gNativePedestrians.emplace_back();
            auto& actor=gNativePedestrians[plan.slot];actor={};
            actor.node=plan.node;actor.nextNode=nodes[plan.node].links.front().target;
            actor.position=plan.point;
            actor.groundOffset=plan.point.z-nodes[plan.node].position.z;
            actor.yaw=std::atan2(gPlayerWorldY-plan.point.y,gPlayerWorldX-plan.point.x)*57.2957795f;
            auto& ped=gPlayTestRuntime.pedestrians[plan.slot];ped=StorylandRuntimePed{};
            ped.model=plan.model;ped.stats=gPlayTestPedestrianAssets[plan.model].pedStats;
            ped.stats.pedType=gangSlot==StorylandExtendedHaitianGang::populationKind?
                StorylandExtendedHaitianGang::pedestrianType:7+gangSlot;
            ped.extendedGangIdentity=gangSlot==StorylandExtendedHaitianGang::populationKind?gangSlot:-1;
            ped.health=float(std::max(1,gang.health));ped.armour=float(std::max(0,gang.armour));
            ped.hostile=true;ped.personallyProvoked=true;ped.state=StorylandPedState::Combat;
            ped.weapon=plan.weapon;ped.ammunition=90;
            ped.magazine=gPlayTestRuntime.weapons[plan.weapon].clipSize;
            if(plan.slot<gPlayTestPedestrianBlends.size())gPlayTestPedestrianBlends[plan.slot]={};
            const uint64_t id=0x8000000000000000ULL|++actorSerial_;
            ped.turfWarActorId=id;
            spawned_[id]={plan.slot,battle.defender};spawnedIds.push_back(id);
        }
        return true;
    }

    void clearGangWarEnemies(const std::vector<std::uint64_t>& actorIds) override {
        for(std::uint64_t id:actorIds) {
            const auto it=spawned_.find(id);
            if(it==spawned_.end())continue;
            const size_t slot=it->second.slot;
            if(slot<gPlayTestRuntime.pedestrians.size()&&
                gPlayTestRuntime.pedestrians[slot].turfWarActorId==id) {
                auto& ped=gPlayTestRuntime.pedestrians[slot];
                ped.turfWarActorId=0;
                if(ped.health>0) {
                    ped.hostile=ped.playerDamaged;
                    ped.personallyProvoked=ped.playerDamaged;
                    ped.state=ped.hostile?StorylandPedState::Combat:StorylandPedState::Walk;
                }
            }
            spawned_.erase(it);
        }
    }
    void announceGangTurfEvent(const std::string& message) override {
        message_=message;
        messageUntil_=gPlayTestRuntime.elapsed+5.0f;
    }
    void payAdditionalGangTurfIncome(int amount) override {
        if(amount<=0)return;
        gPlayTestRuntime.money=int32_t(std::clamp<int64_t>(
            int64_t(gPlayTestRuntime.money)+amount,INT32_MIN,INT32_MAX));
        gPlayTestMissionEnvironment.money=gPlayTestRuntime.money;
        save();
    }
    void gangTurfOwnershipChanged(const storyland::VcsTurf&,storyland::VcsTurfGang) override {
        save();
    }

private:
    static const StorylandModGangTurfConfig* gangTurfConfig() {
        const auto* loader=gDtzArchive.modloader();
        if(!loader||!loader->gangTurfConfig().enabled)return nullptr;
        return &loader->gangTurfConfig();
    }
    std::string haitianWarning() const {
        return haitianAssetError_.empty()?
            "The requested supplemental gang models are unavailable.":
            haitianAssetError_;
    }
    static void retireHaitianPopulation() {
        for(auto& ped:gPlayTestRuntime.pedestrians)
            if(ped.scriptHandle<0&&ped.extendedGangIdentity==StorylandExtendedHaitianGang::populationKind&&
                ped.turfWarActorId==0) {
                ped.renderAlpha=0;
                ped.hostile=false;
                ped.personallyProvoked=false;
                ped.extendedGangIdentity=-1;
            }
    }
    void loadHaitianModels() {
        if(gPlayTestRuntime.extendedHaitianGang.available)return;
        std::string error;
        if(!bindHaitianModels(error))haitianAssetError_=std::move(error);
    }
    struct WavePed {size_t slot=0;storyland::VcsTurfGang gang=storyland::VcsTurfGang::Cholos;};
    static std::string upper(std::string name) {
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char value) {
            return char(std::toupper(value));
        });
        return name;
    }
    static bool namedLike(const std::string& name,std::initializer_list<const char*> aliases) {
        const auto normalized=upper(name);
        for(const char* alias:aliases)
            if(normalized.find(alias)!=std::string::npos)return true;
        return false;
    }
    static bool readZone(const StorylandNativeWorld& world,
        std::initializer_list<const char*> names,storyland::VcsTurfRect& bounds) {
        const StorylandNativeZone* selected=nullptr;
        const auto scan=[&](const auto& zones) {
            for(const auto& zone:zones) {
                if(!namedLike(zone.name,names))continue;
                const float width=zone.maximum.x-zone.minimum.x;
                const float height=zone.maximum.y-zone.minimum.y;
                if(width<=0||height<=0)continue;
                if(!selected || width*height>
                    (selected->maximum.x-selected->minimum.x)*
                    (selected->maximum.y-selected->minimum.y))selected=&zone;
            }
        };
        scan(world.zones.navigation);
        if(!selected)scan(world.zones.information);
        if(!selected)scan(world.zones.map);
        if(!selected)return false;
        bounds={selected->minimum.x,selected->minimum.y,
            selected->maximum.x,selected->maximum.y};
        return bounds.valid();
    }
    bool initialize(const StorylandNativeWorld& world,bool restoreLegacy=false) {
        using namespace storyland;
        const auto* config=gangTurfConfig();
        if(!config) {error_="No enabled gang-turf data mod is mounted.";return false;}
        std::vector<VcsTurfDistrictZone> districts;
        const auto addDistrict=[&](VcsTurfDistrict district,
            std::initializer_list<const char*> names) {
            VcsTurfRect bounds;
            if(!readZone(world,names,bounds))return false;
            districts.push_back({district,bounds});return true;
        };
        const auto addRestricted=[&](VcsTurfRestrictedArea area,
            std::initializer_list<const char*> names,
            std::vector<VcsTurfRestrictedZone>& restricted) {
            VcsTurfRect bounds;
            if(!readZone(world,names,bounds))return false;
            restricted.push_back({area,bounds});return true;
        };
        if(!addDistrict(VcsTurfDistrict::LittleHaiti,{"HAITI"}) ||
            !addDistrict(VcsTurfDistrict::LittleHavana,{"HAVANA"}) ||
            !addDistrict(VcsTurfDistrict::Downtown,{"DTOWN","DOWNT"}) ||
            !addDistrict(VcsTurfDistrict::Viceport,{"DOCKS","VICEPT","VICEPORT"})) {
            error_="VCS native zone data missing mainland gang district";
            return false;
        }
        std::vector<VcsTurfRestrictedZone> restricted;
        for(const auto& zone:world.zones.information) {
            if(!namedLike(zone.name,{"AIRPORT","TERMINL","BAGGAG","ARMYBAS"}))continue;
            const VcsTurfRect bounds{zone.minimum.x,zone.minimum.y,zone.maximum.x,zone.maximum.y};
            if(bounds.valid())restricted.push_back({namedLike(zone.name,{"ARMYBAS"})?
                VcsTurfRestrictedArea::FortBaxter:VcsTurfRestrictedArea::EscobarAirport,bounds});
        }
        const auto hasRestricted=[&](VcsTurfRestrictedArea area) {
            return std::any_of(restricted.begin(),restricted.end(),
                [&](const auto& value){return value.area==area;});
        };
        if((!hasRestricted(VcsTurfRestrictedArea::FortBaxter)&&
                !addRestricted(VcsTurfRestrictedArea::FortBaxter,{"ARMYBAS","ARMYB","FORTBAX"},restricted))||
            (!hasRestricted(VcsTurfRestrictedArea::EscobarAirport)&&
                !addRestricted(VcsTurfRestrictedArea::EscobarAirport,{"A_PORT","AIRPORT","ESCOBAR"},restricted))||
            !addRestricted(VcsTurfRestrictedArea::StarfishIsland,{"STARI","STARISL","STARFISH"},restricted)) {
            error_="VCS native zone data missing a protected gang-turf area";
            return false;
        }
        for(const auto& excluded:config->excludedZones) {
            const auto append=[&](const auto& zones) {
                for(const auto& zone:zones) {
                    if(upper(zone.name)!=upper(excluded))continue;
                    const VcsTurfRect bounds{zone.minimum.x,zone.minimum.y,zone.maximum.x,zone.maximum.y};
                    const auto duplicate=std::any_of(restricted.begin(),restricted.end(),[&](const auto& item) {
                        return item.bounds.minX==bounds.minX&&item.bounds.minY==bounds.minY&&
                            item.bounds.maxX==bounds.maxX&&item.bounds.maxY==bounds.maxY;
                    });
                    if(bounds.valid()&&!duplicate)restricted.push_back({VcsTurfRestrictedArea::Additional,bounds});
                }
            };
            append(world.zones.information);append(world.zones.navigation);append(world.zones.map);
        }
        const std::array<std::string,4> districtNames{"HAITI","HAVANA","DTOWN","DOCKS"};
        std::vector<VcsTurf> territories;
        for(const auto& definition:config->territories) {
            const auto district=std::find(districtNames.begin(),districtNames.end(),upper(definition.districtZone));
            if(district==districtNames.end()) {
                error_="Gang turf data names a district outside the permitted mainland areas.";
                return false;
            }
            const auto kind=static_cast<VcsTurfDistrict>(district-districtNames.begin());
            const auto native=std::find_if(districts.begin(),districts.end(),
                [&](const auto& zone){return zone.district==kind;});
            const auto& fraction=definition.bounds;
            if(native==districts.end()||!std::all_of(fraction.begin(),fraction.end(),
                [](float value){return std::isfinite(value)&&value>=0&&value<=1;})||
                fraction[0]>=fraction[2]||fraction[1]>=fraction[3]) {
                error_="Gang turf data contains invalid normalized district bounds.";
                return false;
            }
            VcsTurf turf;
            turf.id=definition.id;turf.displayName=definition.name;turf.district=kind;
            const float width=native->bounds.maxX-native->bounds.minX;
            const float height=native->bounds.maxY-native->bounds.minY;
            turf.bounds={native->bounds.minX+fraction[0]*width,native->bounds.minY+fraction[1]*height,
                fraction[2]==1?native->bounds.maxX:native->bounds.minX+fraction[2]*width,
                fraction[3]==1?native->bounds.maxY:native->bounds.minY+fraction[3]*height};
            const auto owner=upper(definition.owner);
            if(owner=="CHOLOS")turf.owner=VcsTurfGang::Cholos;
            else if(owner=="HAITIANS")turf.owner=VcsTurfGang::Haitians;
            else if(owner=="VANCE")turf.owner=VcsTurfGang::Vance;
            else {error_="Gang turf data names an unsupported owner.";return false;}
            turf.dailyIncome=definition.dailyIncome;
            const bool excluded=std::any_of(restricted.begin(),restricted.end(),
                [&](const auto& area){return turf.bounds.overlaps(area.bounds);});
            if(!excluded)territories.push_back(std::move(turf));
        }
        initialized_=turfs_.initializeTerritories(territories,restricted,&error_);
        if(initialized_) {
            lifeGenerations_.resize(gPlayTestRuntime.pedestrians.size(),1);
            previouslyAlive_.resize(gPlayTestRuntime.pedestrians.size(),false);
            for(size_t i=0;i<gPlayTestRuntime.pedestrians.size();++i)
                previouslyAlive_[i]=gPlayTestRuntime.pedestrians[i].health>0;
            if(restoreLegacy)load();
            error_=findGangSlot(VcsTurfGang::Haitians)<0?
                haitianWarning():std::string();
        }
        return initialized_;
    }
    static int32_t findGangSlot(storyland::VcsTurfGang faction) {
        const auto& gangs=gPlayTestMissionEnvironment.gangs;
        if(faction!=storyland::VcsTurfGang::Haitians) {
            const int32_t slot=faction==storyland::VcsTurfGang::Cholos?0:6;
            return gangs[size_t(slot)].pedModels[0]>0||gangs[size_t(slot)].pedModels[1]>0?slot:-1;
        }
        if(gPlayTestRuntime.extendedHaitianGang.available)
            return StorylandExtendedHaitianGang::populationKind;
        return -1;
    }
    static size_t findGangModel(int32_t gangSlot,size_t ordinal) {
        const bool customGang=gangSlot==StorylandExtendedHaitianGang::populationKind&&
            gPlayTestRuntime.extendedHaitianGang.available;
        if(!customGang&&(gangSlot<0||size_t(gangSlot)>=gPlayTestMissionEnvironment.gangs.size()))return SIZE_MAX;
        const auto& models=customGang?gPlayTestRuntime.extendedHaitianGang.pedModels:
            gPlayTestMissionEnvironment.gangs[size_t(gangSlot)].pedModels;
        for(size_t attempt=0;attempt<models.size();++attempt) {
            const auto id=models[(ordinal+attempt)%models.size()];
            for(size_t i=0;i<gPlayTestPedestrianAssets.size();++i)
                if(id>0&&gPlayTestPedestrianAssets[i].pedStats.valid&&
                    gPlayTestPedestrianAssets[i].pedStats.modelId==uint32_t(id))return i;
        }
        return SIZE_MAX;
    }
    int32_t populationGangAt(const StorylandNativePoint& point,int32_t original) const {
        if(!gameplayUnlocked()||!initialized_||!turfs_.enabled()||
            original<0||original>=9)return original;
        const auto* turf=turfs_.turfAt({point.x,point.y});
        if(!turf)return original;
        if(original==StorylandFortBaxterRetailResponse::GangId||original==4)return original;
        const auto owner=findGangSlot(turf->owner);
        return owner>=0?owner:original;
    }
    size_t availableSlot(const std::vector<size_t>& reserved) const {
        const auto& runtime=gPlayTestRuntime;
        for(size_t index=gNativePedestrians.size();index<runtime.pedestrians.size();++index)
            if(std::find(reserved.begin(),reserved.end(),index)==reserved.end())return index;
        for(size_t index=0;index<gNativePedestrians.size()&&index<runtime.pedestrians.size();++index) {
            const auto& ped=runtime.pedestrians[index];
            if(std::find(reserved.begin(),reserved.end(),index)!=reserved.end()||
                ped.scriptHandle>=0||ped.fortBaxterSoldier||ped.policePursuing||
                ped.turfWarActorId!=0||int(index)==runtime.targetedPed)continue;
            const auto& actor=gNativePedestrians[index];
            const float distance=std::hypot(actor.position.x-gPlayerWorldX,actor.position.y-gPlayerWorldY);
            if(ped.health<=0&&ped.deathTime>3.0f)return index;
            if(distance>90.0f)return index;
        }
        return SIZE_MAX;
    }
    void registerDeaths() {
        auto& runtime=gPlayTestRuntime;
        if(lifeGenerations_.size()<runtime.pedestrians.size()) {
            lifeGenerations_.resize(runtime.pedestrians.size(),1);
            previouslyAlive_.resize(runtime.pedestrians.size(),false);
        }
        std::vector<std::uint64_t> killed;
        std::vector<size_t> waveSlots;
        for(const auto& item:spawned_)waveSlots.push_back(item.second.slot);
        for(auto& item:spawned_) {
            const size_t slot=item.second.slot;
            if(slot>=runtime.pedestrians.size()||
                runtime.pedestrians[slot].turfWarActorId!=item.first||
                runtime.pedestrians[slot].health<=0)killed.push_back(item.first);
        }
        for(std::uint64_t id:killed) {
            const auto entry=spawned_.find(id);
            if(entry==spawned_.end())continue;
            const auto slot=entry->second.slot;
            const auto faction=entry->second.gang;
            storyland::VcsTurfPoint point{gPlayerWorldX,gPlayerWorldY};
            bool playerKill=false;
            if(slot<gNativePedestrians.size()&&slot<runtime.pedestrians.size()) {
                auto& ped=runtime.pedestrians[slot];
                const auto& pos=gNativePedestrians[slot].position;
                point={pos.x,pos.y};
                if(ped.turfWarActorId==id) {playerKill=ped.playerKillCounted;ped.turfWarActorId=0;}
            }
            spawned_.erase(id);
            turfs_.gangMemberKilled({id,faction,point,playerKill},*this);
        }
        for(size_t i=0;i<runtime.pedestrians.size()&&i<gNativePedestrians.size();++i) {
            const auto& ped=runtime.pedestrians[i];
            const bool alive=ped.health>0;
            if(!alive&&previouslyAlive_[i]&&ped.playerKillCounted) {
                const bool wave=std::find(waveSlots.begin(),waveSlots.end(),i)!=waveSlots.end();
                if(!wave&&ped.model<gPlayTestPedestrianAssets.size()) {
                    const auto cholo=findGangSlot(storyland::VcsTurfGang::Cholos);
                    const auto haitian=findGangSlot(storyland::VcsTurfGang::Haitians);
                    const int32_t nativeSlot=ped.stats.valid?ped.stats.pedType-7:-1;
                    const bool choloMember=cholo>=0&&nativeSlot==cholo&&ped.extendedGangIdentity<0;
                    const bool haitianMember=haitian==StorylandExtendedHaitianGang::populationKind&&
                        ped.extendedGangIdentity==StorylandExtendedHaitianGang::populationKind;
                    if(ped.scriptHandle<0&&!ped.fortBaxterSoldier&&
                        (choloMember||haitianMember)) {
                        const auto gang=choloMember?storyland::VcsTurfGang::Cholos:
                            storyland::VcsTurfGang::Haitians;
                        const auto& point=gNativePedestrians[i].position;
                        turfs_.gangMemberKilled({(std::uint64_t(i+1)<<32)|lifeGenerations_[i],
                            gang,{point.x,point.y},true},*this);
                    }
                }
            }
            if(alive&&!previouslyAlive_[i])++lifeGenerations_[i];
            previouslyAlive_[i]=alive;
        }
    }
    void save() {
        if(!initialized_)return;
        std::error_code directoryError;
        const auto folder=storylandUserFilesDirectory(true);
        if(folder.empty())return;
        std::filesystem::create_directories(folder,directoryError);
        if(directoryError)return;
        std::ostringstream output;
        if(!turfs_.writeSave(output))return;
        const auto text=output.str();
        const std::vector<uint8_t> bytes(text.begin(),text.end());
        std::string writeError;
        if(!storylandWriteFilesTransaction({{folder/"Storyland VCS Gang Turfs.recovery.dat",&bytes}},writeError))
            error_=writeError;
    }
    void load() {
        const auto path=storylandUserFilesDirectory(true)/"Storyland VCS Gang Turfs.dat";
        std::ifstream input(path,std::ios::binary);
        if(input)turfs_.readSave(input);
    }
    storyland::VcsGangTurfs turfs_;
    std::map<std::uint64_t,WavePed> spawned_;
    std::vector<std::uint32_t> lifeGenerations_;
    std::vector<bool> previouslyAlive_;
    std::uint64_t actorSerial_=0;
    bool initialized_=false;
    bool haitianModelsAttempted_=false;
    std::string haitianAssetError_;
    float lastElapsed_=-1.0f;
    double lastDayMinutes_=-1.0;
    double totalGameMinutes_=0.0;
    std::string message_;
    float messageUntil_=0.0f;
    std::string error_;
};
