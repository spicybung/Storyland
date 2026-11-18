#pragma once
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cmath>

inline constexpr float StorylandRetailEngineRange = 37.0f;
inline constexpr float StorylandRetailDoorRange = 50.0f;

struct StorylandVehicleSirenAudio {
    uint32_t sample=UINT32_MAX,frequency=0;
    const char* name=nullptr;
};

inline StorylandVehicleSirenAudio storylandLcsVehicleSirenAudio(uint32_t modelId) {
    // SLUS-21423 3B2BE0: 28-byte settings, model 130 first; siren at +10/+14.
    switch(modelId) {
    case 138:return {251,10588,"sfxpolicesirenslow"};
    case 147:return {5,12688,"sfxambulancesi"};
    case 148:return {251,11471,"sfxpolicesirenslow"};
    case 157:return {251,10511,"sfxpolicesirenslow"};
    case 158:return {251,11029,"sfxpolicesirenslow"};
    default:return {};
    }
}

inline bool storylandLcsVehicleDoorType(uint32_t modelId,uint8_t& type) {
    // SLUS-21423 0x3B2BE0, aVehicleSettings[model - MI_FIRST_VEHICLE].
    static constexpr uint8_t types[]={
        1,0,0,1,2,0,1,2,2,2,1,0,1,0,0,0,1,0,1,0,
        0,1,0,0,0,0,0,1,0,0,1,3,2,2,1,3,0,1,0,1,
        2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,2,0,
        0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,2,1,2
    };
    if(modelId<130||modelId-130>=sizeof(types)/sizeof(types[0]))return false;
    type=types[modelId-130];return true;
}

inline uint32_t storylandLcsVehicleDoorSample(uint8_t type,bool opening) {
    // SLUS-21423 0x1A40D8..0x1A42F0, SFX0_PS slots.
    static constexpr uint16_t samples[][2]={{231,230},{228,227},{299,298},{0,0}};
    return type<4?samples[type][opening?0:1]:UINT32_MAX;
}

inline uint32_t storylandVcsVehicleDoorSample(uint8_t type,bool opening) {
    // SLUS-21590 0x1BBC90..0x1BBE74, SFX0_PS slots.
    static constexpr uint16_t samples[][2]={{219,218},{217,216},{279,278},{0,0}};
    return type<4?samples[type][opening?0:1]:UINT32_MAX;
}

inline uint32_t storylandVcsPspVehicleDoorSample(uint8_t type,bool opening) {
    // Original SFX0_PSP VAGs match the corresponding LCS PS2 VAGs byte for byte.
    static constexpr uint16_t samples[][2]={{260,259},{257,256},{334,333},{0,0}};
    return type<4?samples[type][opening?0:1]:UINT32_MAX;
}

inline uint8_t storylandRetailSoundVolume(uint8_t emittingVolume, float range, float distance) {
    // SLUS-21423 0x238A10 and SLUS-21590 0x2A7120.
    if (!(range > 0 && distance < range)) return 0;
    const float nearDistance = .2f * range;
    if (distance < nearDistance) return std::min<uint8_t>(127, emittingVolume);
    const float interval = range - nearDistance;
    const float ratio = (interval - (distance - nearDistance)) / interval;
    return uint8_t(std::min(127.0f, ratio * ratio * emittingVolume));
}

inline float storylandVcsAudioDistance(float x,float y,float z) {
    // Original1B56C0 measures from the camera and compresses the vertical axis.
    z*=.65f;
    return std::sqrt((x*x+y*y)+z*z);
}

struct StorylandVcsBoboAudioState {
    float accelerator=0;
    bool wheelsOnGround=false;
};

struct StorylandVcsBoboMotorSound {
    uint8_t volume=0;
    uint32_t frequency=4270;
};

inline StorylandVcsBoboMotorSound storylandVcsBoboMotorSound(
    StorylandVcsBoboAudioState& state,float forwardVelocity,float maximumVelocity,
    float throttle,bool wheelsOnGround,uint32_t entity,float distance) {
    // Original1B9360 and model208 branch1B8C60..1B9330. Velocities are native units.
    const float accelerator=std::fabs(throttle);
    state.accelerator=state.accelerator<accelerator?
        std::min(accelerator,state.accelerator+.09f):std::max(accelerator,state.accelerator-.07f);
    if(distance<StorylandRetailEngineRange&&!wheelsOnGround&&state.wheelsOnGround)
        state.accelerator*=.4f;
    state.wheelsOnGround=wheelsOnGround;
    const float ratio=wheelsOnGround?(maximumVelocity>0?
        std::min(1.0f,std::fabs(forwardVelocity/maximumVelocity)):0):state.accelerator;
    StorylandVcsBoboMotorSound sound;
    sound.volume=storylandRetailSoundVolume(uint8_t(ratio*100.0f),StorylandRetailEngineRange,distance);
    sound.frequency=4270u+uint32_t(ratio*2130.0f)+(entity*20u)%100u;
    return sound;
}

inline constexpr float StorylandRetailSkidRange = 40.0f;

inline uint8_t storylandRetailSkidVolume(float slip, float distance) {
    // LCS 0x1A2D88, VCS 0x1BA7B8. The caller supplies wheel slip.
    const auto volume = uint8_t(std::clamp(slip, 0.0f, 1.0f) * 55.0f);
    return storylandRetailSoundVolume(volume, StorylandRetailSkidRange, distance);
}

inline float storylandRetailSkidFrequency(float slip) {
    // Default road sample. Other surface/vehicle branches have separate rates.
    return 11000.0f + 5000.0f * std::clamp(slip, 0.0f, 1.0f);
}

// LCS vehicle settings, reLCS lcs 33abd1b4e7a7b19e2d09c796c481c3325c1e2902.
// https://git.shihaam.dev/archivemirrors/re3/src/branch/lcs/src/audio/AudioLogic.cpp
// Indexed by ModelIndices.h (SPIDER=130), not the obsolete eVehicleModel enum.
// Symbolic names only: native VAG slots must be resolved from their own bank.
struct StorylandVehicleAudioProfile {
    uint32_t modelId;
    const char* idle;
    const char* rev;
    const char* horn;
    uint32_t hornFrequency;
};
inline const StorylandVehicleAudioProfile* storylandLcsVehicleAudio(uint32_t modelId) {
    static constexpr StorylandVehicleAudioProfile profiles[] = {
        {130, "sfxcaridleporshe", "sfxcarrevporshe", "sfxcarhornporsche", 11025}, // SPIDER
        {131, "sfxcaridlepathfinder", "sfxcarrevpathfinder", "sfxcarhornjeep", 26513}, // LANDSTAL
        {132, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhorn56chev", 11487}, // IDAHO
        {133, "sfxcaridleporshe", "sfxcarrevporshe", "sfxcarhornporsche", 11025}, // STINGER
        {134, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhorntruck", 29711}, // LINERUN
        {135, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhorn56chev", 12893}, // PEREN
        {136, "sfxcaridlemerc", "sfxcarrevmerc", "sfxcarhornbmw328", 10706}, // SENTINEL
        {137, "sfxcaridlepathfinder", "sfxcarrevpathfinder", "sfxcarhornjeep", 26513}, // PATRIOT
        {138, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhorntruck", 29711}, // FIRETRUCK
        {139, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhorntruck", 31478}, // TRASH
        {140, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhornbmw328", 9538}, // STRETCH
        {141, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhorn56chev", 10842}, // MANANA
        {142, "sfxcaridleporshe", "sfxcarrevporshe", "sfxcarhornporsche", 11025}, // INFERNUS
        {143, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhorn56chev", 11487}, // BLISTA
        {144, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornbus2", 18000}, // PONY
        {145, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornbus", 18286}, // MULE
        {146, "sfxcaridleporshe", "sfxcarrevporshe", "sfxcarhornporsche", 11025}, // CHEETAH
        {147, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornjeep", 22295}, // AMBULAN
        {148, "sfxcaridlemerc", "sfxcarrevmerc", "sfxcarhornporsche", 9271}, // FBICAR
        {149, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhorn56chev", 12170}, // MOONBEAM
        {150, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhornbmw328", 11000}, // ESPERANT
        {151, "sfxcaridlepathfinder", "sfxcarrevpathfinder", "sfxcarhornbmw328", 10796}, // TAXI
        {152, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhorn56chev", 11487}, // KURUMA
        {153, "sfxcaridlepathfinder", "sfxcarrevpathfinder", "sfxcarhornpickup", 10924}, // BOBCAT
        {154, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornpickup", 11025}, // MRWHOOP
        {155, "sfxcaridlehotrod", "sfxcarrevhotrod", "sfxcarhornjeep", 26513}, // BFINJECT
        {156, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhorn56chev", 11487}, // HEARSE
        {157, "sfxcaridlemerc", "sfxcarrevmerc", "sfxcarhornbmw328", 10706}, // POLICE
        {158, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornbus", 17260}, // ENFORCER
        {159, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornpickup", 8670}, // SECURICA
        {160, "sfxcaridlecobra", "sfxcarrevcobra", "sfxcarhornporsche", 10400}, // BANSHEE
        {161, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhornbus2", 11652}, // BUS
        {162, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhorntruck", 29711}, // RHINO
        {163, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhorntruck", 28043}, // BARRACKS
        {165, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhornbus", 16291}, // COACH
        {166, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhorn56chev", 10842}, // CABBIE
        {167, "sfxcaridlepacard", "sfxcarrevpacard", "sfxcarhorn56chev", 10233}, // STALLION
        {168, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornpickup", 8670}, // RUMPO
        {170, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhorntruck", 31478}, // BELLYUP
        {171, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhorntruck", 31478}, // MRWONGS
        {172, "sfxcaridlepont", "sfxcarrevpont", "sfxcarhornbmw328", 9003}, // MAFIA
        {173, "sfxcaridlecobra", "sfxcarrevcobra", "sfxcarhornjeep", 26513}, // YARDIE
        {174, "sfxcaridleporshe", "sfxcarrevporshe", "sfxcarhornporsche", 11025}, // YAKUZA
        {175, "sfxcaridlehotrod", "sfxcarrevhotrod", "sfxcarhorn56chev", 11487}, // DIABLOS
        {176, "sfxcaridlepathfinder", "sfxcarrevpathfinder", "sfxcarhornpickup", 10924}, // COLUMB
        {177, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornpickup", 8670}, // HOODS
        {178, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornpickup", 8670}, // PANLANT
        {179, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhorntruck", 28043}, // FLATBED
        {180, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornbus", 18286}, // YANKEE
        {181, "sfxcaridlepont", "sfxcarrevpont", "sfxcarhorn56chev", 10842}, // BORGNINE
        {182, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornbus", 18286}, // TOPFUN
        {183, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornpickup", 8670}, // CAMPVAN
        {184, "sfxcaridlespider", "sfxcarrevspider", "sfxcarhornbus2", 18000}, // BALLOT
        {185, "sfxcaridlecobra", "sfxcarrevcobra", "sfxcarhornporsche", 10400}, // SHELBY
        {186, "sfxcaridlecobra", "sfxcarrevcobra", "sfxcarhornporsche", 10400}, // PONTIAC
        {187, "sfxcaridleporshe", "sfxcarrevporshe", "sfxcarhornporsche", 11025}, // ESPRIT
        {188, "sfxcaridletruck", "sfxcarrevtruck", "sfxcarhorntruck", 31478}, // AMMOTRUK
        {189, "sfxcaridlehotrod", "sfxcarrevhotrod", "sfxcarhornjeep", 25400}, // HOTROD
        {190, "sfxcaridlecobra", "sfxcarrevcobra", "sfxcarhornporsche", 10400}, // SINDACCO_CAR
        {191, "sfxcaridlecobra", "sfxcarrevcobra", "sfxcarhornporsche", 10400}, // FORELLI_CAR
        {202, "sfxcaridlevtwi", "sfxcarrevvtwi", "sfxcarhornjeep", 26313}, // ANGEL
        {203, "sfxmopedidle", "sfxmopedrev", "sfxcarhornjeep", 30000}, // PIZZABOY
        {204, "sfxmopedidle", "sfxmopedrev", "sfxcarhornjeep", 30000}, // NOODLEBOY
        {205, "sfxcaridlesportcar", "sfxcarrevsportcar", "sfxcarhornjeep", 27000}, // PCJ600
        {206, "sfxmopedidle", "sfxmopedrev", "sfxcarhornjeep", 31000}, // FAGGIO
        {207, "sfxcaridlevtwi", "sfxcarrevvtwi", "sfxcarhornpickup", 11000}, // FREEWAY
        {208, "sfxcaridlevtwi", "sfxcarrevvtwi", "sfxcarhornjeep", 26313}, // ANGEL2
        {209, "sfxcaridlehonda", "sfxcarrevhonda", "sfxcarhornjeep", 30000}, // SANCHEZ2
        {210, "sfxcaridlehonda", "sfxcarrevhonda", "sfxcarhornjeep", 30000}, // SANCHEZ
    };
    for (const auto& profile : profiles) if (profile.modelId == modelId) return &profile;
    return nullptr;
}

// SLUS-21590 aEngineSounds at 0x4A9538. These are SFX0_PS sample IDs;
// the bank carried by CVehicleModelInfo chooses the row, not a vehicle-name guess.
inline bool storylandVcsEngineSamples(uint8_t bank,uint32_t& idle,uint32_t& rev) {
    static constexpr uint16_t pairs[][2]={
        {58,81},{60,83},{61,84},{55,78},{64,87},{54,76},
        {51,73},{58,81},{55,78},{57,80},{55,78},{52,74},
        {52,74},{52,74},{52,74},{52,74},{66,89},{211,212},
        {53,75},{62,85},{65,77},{63,86},{65,88},{65,88}
    };
    if(bank<4||bank>=4+sizeof(pairs)/sizeof(pairs[0]))return false;
    idle=pairs[bank-4][0];rev=pairs[bank-4][1];return true;
}

inline size_t storylandVcsBankSampleCount(unsigned bank) {
    // CDSTREAM.IRX stores these material-bank boundaries at0x3E98..0x3ED0.
    static constexpr uint8_t materialCounts[]={4,5,5,5,4,4};
    if(bank>=61&&bank<=66)return materialCounts[bank-61];
    // CDSTREAM bank boundaries, also used by the EE bank loader.
    static constexpr uint16_t first[]={
        0,292,293,5819,5827,5830,5833,5836,5839,5842,5845,5848,
        5851,5854,5857,5860,5863,5866,5869,5871,5872,5876,5880,
        5884,5888,5891,5895,5896,5897,5898,5899,5903,5904,5905,
        5906,5907,5912,5913,5914,5915,5916
    };
    return bank+1<sizeof(first)/sizeof(first[0])?first[bank+1]-first[bank]:0;
}
