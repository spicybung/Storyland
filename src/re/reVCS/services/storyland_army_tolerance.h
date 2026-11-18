#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

// Recovered from VCS MAIN.SCM, 2026-11-05, offsets 0xC2B4..0xC4AB:
// 01A9 model 97; *10; 01A9 model 98; *10; 01A8 reset kill counts.
// Local[1] > 31 triggers the hostile branch (gang 3, opcode 0367, value 1).
// Outside both AIRPORT and ARMYBAS the script WAITs 500 ms and subtracts 1.
// Gang 3 flag 0 and the player character's bit 3 restore the protected state.
// This is measured SCM control-flow, NOT a claim about EE-level AI fire latency.
struct StorylandFortBaxterRetailResponse {
    static constexpr uint32_t SoldierModel97=97u;
    static constexpr uint32_t SoldierModel98=98u;
    static constexpr unsigned ToleratedSoldierDeaths=3u;
    static constexpr float HeatPerSoldierDeath=10.0f;
    static constexpr float MissionHostilityCutoff=31.0f;
    static constexpr float OutsideZoneDecaySeconds=0.500f;
    static constexpr uint16_t PlayerProtectionBit=uint16_t(1u<<3);
    static constexpr int GangId=3;

    static bool trackedModel(int32_t model) {
        return model==int32_t(SoldierModel97)||model==int32_t(SoldierModel98);
    }
    static bool alarmFromDeaths(uint32_t deaths) { return deaths>ToleratedSoldierDeaths; }
    static bool alarmFromMissionHeat(float heat) {
        return std::isfinite(heat)&&heat>MissionHostilityCutoff;
    }
    static bool inHostilityRectangle(float x,float y) {
        if(!std::isfinite(x)||!std::isfinite(y))return false;
        const bool areaA=x>=-1780.62744140625f&&x<=-1588.4542236328125f&&
            y>=-304.4773864746094f&&y<=-86.18800354003906f;
        const bool excluded=x>=-1744.94580078125f&&x<=-1691.2762451171875f&&
            y>=-309.347900390625f&&y<=-294.2143859863281f;
        return areaA&&!excluded;
    }
    static bool hostileFromRetailScript(bool scriptAttackWithPolice,
                                         uint16_t playerFlags) {
        // Opcode 0367 toggles Army gang 3's retaliation. Opcode 0500/0501
        // sets/clears the player's bit 3 in the matching SCM branches.
        return scriptAttackWithPolice && !(playerFlags & PlayerProtectionBit);
    }
    static bool uniformProtects(float heat,bool armyFatigues,bool alarm,
                                bool retailScriptObserved=false) {
        return armyFatigues&&!alarm&&
            (retailScriptObserved||!alarmFromMissionHeat(heat));
    }
    static void recordSoldierDeath(uint32_t& total,uint32_t& pending,int32_t model) {
        if(!trackedModel(model))return;
        if(total<std::numeric_limits<uint32_t>::max())++total;
        if(pending<std::numeric_limits<uint32_t>::max())++pending;
    }
    // Standalone monitor for Play Test or a MAIN.SCM that has not launched
    // the native Army control thread. When MAIN.SCM is active, its gang 3
    // command is authoritative and this score is diagnostic only.
    static void tick(float elapsed,float& heat,float& timeOrigin,
                     uint32_t& pending,bool insideZone,bool inHostilityArea,
                     bool& wasInsideZone) {
        if(!std::isfinite(elapsed)||elapsed<0.0f)return;
        if(!std::isfinite(heat)||heat<0.0f)heat=0.0f;
        if(!std::isfinite(timeOrigin)||timeOrigin>elapsed)timeOrigin=elapsed;
        if(insideZone){
            if(inHostilityArea&&pending){
                // Two separate 01A9 queries and one 01A8 reset in the retail
                // script. Pending deaths are consumed only at the authoring
                // rectangle, not on damage or in the spawn callback.
                heat=std::min(float(std::numeric_limits<int32_t>::max()),
                    heat+float(pending)*HeatPerSoldierDeath);
                pending=0;
            }
            timeOrigin=elapsed;wasInsideZone=true;
            return;
        }
        if(wasInsideZone){timeOrigin=elapsed;wasInsideZone=false;return;}
        const double seconds=double(elapsed)-double(timeOrigin);
        if(seconds<double(OutsideZoneDecaySeconds))return;
        const unsigned steps=uint32_t(std::min(2147483647.0,
            std::floor(seconds/double(OutsideZoneDecaySeconds))));
        heat=std::max(0.0f,heat-float(steps));
        timeOrigin=float(double(timeOrigin)+double(steps)*double(OutsideZoneDecaySeconds));
    }
};
