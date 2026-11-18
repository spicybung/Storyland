#pragma once
#include "src/storyland/storyland_play_test_runtime.h"
#include <algorithm>
#include <cctype>

// Values evaluated from reLCS AudioSamples.h, not its stale numeric comments.
// This profile is ONLY for the inherited 20-byte/9941-entry PC SFX table.
// It does not establish the sample order of a Stories PS2/PSP 12-byte bank.
inline bool storylandRelcsCompatibleBank(size_t stride,size_t count) {return stride==20&&count==9941;}
inline int storylandRelcsReferenceSample(StorylandPlayTestSound type,std::string weapon,uint32_t variation) {
    std::transform(weapon.begin(),weapon.end(),weapon.begin(),[](unsigned char c){return char(std::tolower(c));});
    switch(type) {
    case StorylandPlayTestSound::Footstep:return 42+int(variation%5);
    case StorylandPlayTestSound::Land:return 333;
    case StorylandPlayTestSound::VehicleEnter:return 17;
    case StorylandPlayTestSound::VehicleExit:return 18;
    case StorylandPlayTestSound::WaterSplash:return 516+int(variation%4);
    case StorylandPlayTestSound::WeaponPickup:return 361;
    case StorylandPlayTestSound::Melee:return 167;
    case StorylandPlayTestSound::Explosion:return 47+int(variation%3);
    case StorylandPlayTestSound::WeaponReload:
        if(weapon.find("rocket")!=std::string::npos) return 79;
        if(weapon.find("ak")!=std::string::npos) return 78;
        return 77;
    case StorylandPlayTestSound::WeaponShot:
        if(weapon.find("python")!=std::string::npos||weapon.find("revolver")!=std::string::npos) return 88;
        if(weapon.find("colt")!=std::string::npos||weapon.find("pistol")!=std::string::npos) return 50;
        if(weapon.find("spas")!=std::string::npos) return 85;
        if(weapon.find("shot")!=std::string::npos||weapon.find("stubby")!=std::string::npos) return 66;
        if(weapon.find("ak47")!=std::string::npos) return 52;
        if(weapon.find("mp5")!=std::string::npos) return 90;
        if(weapon.find("uzi")!=std::string::npos) return 54;
        if(weapon.find("tec")!=std::string::npos||weapon.find("ingram")!=std::string::npos) return 71;
        if(weapon.find("m4")!=std::string::npos||weapon.find("ruger")!=std::string::npos) return 74;
        if(weapon.find("sniper")!=std::string::npos) return 57;
        if(weapon.find("rocket")!=std::string::npos) return 59;
        if(weapon.find("flame")!=std::string::npos) return 62;
        return -1;
    default:return -1;
    }
}
