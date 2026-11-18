#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// reLCS Ped::LoadNonStandardPedAnim and AnimManager associations. Script wait
// state IDs select named banks and clips, not raw animation IDs or group IDs.
struct StorylandLcsWaitAnimationDefinition {
    int32_t state;
    const char* bank;
    const char* clip;
    bool repeat;
    bool partial=true;
};
inline constexpr std::array<StorylandLcsWaitAnimationDefinition,50> StorylandLcsWaitAnimations{{
    {36,"mpnote","mpnote",false},
    {37,"mpnote","mpnoteloop",true},
    {38,"cs_misc","idle_look",false},
    {39,"cs_misc","idle_no",false},
    {40,"cs_misc","idle_yes",false},
    {41,"cs_misc","idle_chat2",false},
    {42,"cs_misc","idle_cough",false},
    {43,"cs_misc","idle_giggle_female",false},
    {44,"cs_misc","idle_tough_chat",false},
    {45,"cs_misc","idle_cell_talk",false},
    {46,"donh2","donh2_camera",false},
    {47,"donh3","donh3_happy",false},
    {48,"jdt2","jdt2_anxious_talk",false},
    {49,"jdt2","jdt2_shrug",false},
    {50,"jdt4","jdt4_dildo_talk",false},
    {51,"jdt5","jdt5_calm_down",false},
    {52,"jdt5","jdt5_point",false},
    {53,"jdt6","jdt6_pickup",false},
    {54,"mar1","mar1_skirt",false},
    {55,"mar2","mar2_cell_answer",false},
    {56,"mar2","mar2_cell_end",false},
    {57,"mar2","mar2_cell_talk",false},
    {58,"mar2","mar2_foot_tap",false},
    {59,"mar3","mar3_hoochy",false},
    {60,"mar3","mar3_note_idle",false},
    {61,"mar3","mar3_note_pick",false},
    {62,"sal1","sal1_briefcase_down",false},
    {63,"sal2","sal2_idle_seated",false},
    {64,"sal2","sal2_seat_to_stand",false},
    {65,"sal3","sal3_seated_talk",false},
    {66,"sal3","sal3_seated_idle",false},
    {67,"sal4","sal4_dustdown",false},
    {68,"sal4","sal4_girlrun",false},
    {69,"sal6","sal6_angry_seated",false},
    {70,"sal6","sal6_idle_seated",false},
    {71,"sal7","sal7_lookout",false},
    {72,"vic2","vic2_point_angry",false},
    {73,"vic3","vic3_waft",false},
    {74,"vic3","vic3_pickup_roll",false},
    {75,"vic4","vic4_carry_box",false},
    {76,"vic4","vic4_cell_look",false},
    {77,"vic4","vic4_crate_idle",false},
    {78,"vic6","vic6_cell_angry",false},
    {79,"tourist","tourist3",false},
    {80,"tourist","tourist2",false},
    {81,"tourist","tourist1",false},
    {82,"mac2","mac2_plead",false},
    {83,"jdt6","jdt6_doorknock",false},
    {84,"sal3","sal3_sit_down",false},
    {85,"vic7","vic7_prod_with_foot",false},
}};
inline const StorylandLcsWaitAnimationDefinition* storylandLcsWaitAnimation(int32_t state) {
    static constexpr StorylandLcsWaitAnimationDefinition roadLook{3,"ped","roadcross",true,true};
    static constexpr StorylandLcsWaitAnimationDefinition duck{14,"ped","duck_down",false,true};
    static constexpr StorylandLcsWaitAnimationDefinition chat{19,"ped","idle_chat",true,true};
    static constexpr StorylandLcsWaitAnimationDefinition sitDown{21,"ped","seat_down",false,true};
    static constexpr StorylandLcsWaitAnimationDefinition sitUp{23,"ped","seat_up",false,true};
    static constexpr StorylandLcsWaitAnimationDefinition sitIdle{24,"ped","seat_idle",true,true};
    static constexpr StorylandLcsWaitAnimationDefinition atm{25,"ped","ATM",false,true};
    static constexpr StorylandLcsWaitAnimationDefinition handsUp{35,"ped","handsup",false,true};
    if(state==3)return &roadLook;
    if(state==14)return &duck;
    if(state==19)return &chat;
    if(state==21)return &sitDown;
    if(state==23)return &sitUp;
    if(state==24)return &sitIdle;
    if(state==25)return &atm;
    if(state==35)return &handsUp;
    const auto found=std::find_if(StorylandLcsWaitAnimations.begin(),StorylandLcsWaitAnimations.end(),
        [state](const auto& definition){return definition.state==state;});
    return found==StorylandLcsWaitAnimations.end()?nullptr:&*found;
}
inline std::vector<std::string> storylandLcsAnimationBankClips(const std::string& bank) {
    std::vector<std::string> names;
    for(const auto& definition:StorylandLcsWaitAnimations)if(bank==definition.bank)names.push_back(definition.clip);
    std::sort(names.begin(),names.end());names.erase(std::unique(names.begin(),names.end()),names.end());
    return names;
}
