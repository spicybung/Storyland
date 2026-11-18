#pragma once

#include <cstddef>
#include <algorithm>
#include <cctype>
#include <string>

inline bool storylandCutsceneRigidPropMayHoldBindPose(const std::string& error,
                                                     bool hasSkinnedVertices,
                                                     std::size_t boneCount,
                                                     std::string modelName = {}) {
    if(error!="Animation has no active authored tracks matching the model skeleton.")
        return false;
    // An unanimated rigid prop (e.g. cssuitcase.mdl) is a scene object, not
    // a failed humanoid. Never hide a genuinely mismatched pedestrian skeleton.
    std::transform(modelName.begin(),modelName.end(),modelName.begin(),
        [](unsigned char ch){return char(std::tolower(ch));});
    const bool suitcase=modelName.find("suitcase")!=std::string::npos ||
        modelName.find("cssuit")!=std::string::npos;
    return !hasSkinnedVertices && (boneCount<=12u || (suitcase && boneCount<=128u));
}

inline bool storylandFortBaxterHostile(bool alarm,bool armyFatigues,
                                      bool scriptedThreat,bool personallyProvoked,
                                      bool introMissionProtected) {
    // A wounded soldier is not an immediate base-wide retaliation trigger.
    // Fatalities are counted by the Fort Baxter mission monitor. Separate
    // mission-script orders and loss of uniform still have their own effect.
    (void)personallyProvoked;
    return alarm||!armyFatigues||(scriptedThreat&&!introMissionProtected);
}
