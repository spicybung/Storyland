#pragma once
#include <array>
#include <cmath>
#include <string>

// VCS PS2 native empire constructor 0x2E3568, registration calls 0x2E3824..0x2E41C8.
// Site order is the retail script index. The final component is radians.
inline constexpr std::array<std::array<float,4>,30> StorylandVcsEmpireSites{{
    {{-933.377991f,-1102.32996f,13.8280001f,3.1415925f}},
    {{-1208.71704f,-649.096008f,10.7110004f,1.69205499f}},
    {{-928.958008f,-686.54303f,10.2030001f,3.22885895f}},
    {{-964.10907f,-539.991577f,9.94972038f,4.82890749f}},
    {{-1151.62903f,-271.010986f,10.3170004f,0.0533517562f}},
    {{-886.721985f,-221.042007f,10.033f,1.54315984f}},
    {{-1201.89294f,-103.446999f,10.4060001f,1.57079637f}},
    {{-962.182983f,194.503998f,8.70300007f,-0.103236236f}},
    {{-704.539856f,616.399841f,10.0507116f,1.45918572f}},
    {{-420.463348f,868.509216f,10.0312462f,4.31644964f}},
    {{-973.635986f,1036.16504f,10.0450001f,1.57079637f}},
    {{-657.551025f,1091.23706f,10.0609999f,4.71238899f}},
    {{-648.04303f,1491.70898f,10.9209995f,4.71238899f}},
    {{-397.808014f,1400.65295f,10.031002f,1.57079637f}},
    {{481.36618f,1308.92822f,16.5569038f,-0.137514502f}},
    {{256.23114f,1079.427f,13.6053705f,2.00127268f}},
    {{288.56015f,899.850952f,10.2992725f,1.42468274f}},
    {{499.566162f,627.308594f,10.022356f,1.57079637f}},
    {{410.417664f,386.938202f,10.6102695f,1.26501071f}},
    {{373.498688f,209.882828f,10.0232706f,1.49729538f}},
    {{283.807007f,-100.275421f,10.3016148f,1.12472391f}},
    {{299.345947f,-294.581299f,10.9366264f,1.57031989f}},
    {{463.861023f,30.3729f,10.0665846f,1.57031989f}},
    {{345.531616f,-431.574402f,9.11637592f,0.757326484f}},
    {{273.639771f,-899.609436f,9.1644249f,4.25575256f}},
    {{203.816071f,-685.912231f,9.46562386f,-0.210099995f}},
    {{-36.8120537f,-977.537415f,10.2246399f,0.0f}},
    {{56.6274529f,-1108.92468f,9.42330074f,1.57079637f}},
    {{-65.1749268f,-1380.80334f,9.42116165f,1.48352993f}},
    {{201.710495f,-1210.16907f,9.40193939f,4.61199999f}},
}};

struct StorylandEmpireVisual {
    std::string building;
    bool construction=false;
    bool detailed=false;
};

inline StorylandEmpireVisual storylandEmpireVisual(int type,int scale,int state,bool damaged,
    const std::array<float,4>& site,float cameraX,float cameraY) {
    StorylandEmpireVisual result;
    if(type<1||type>6||scale<1||scale>3||!std::isfinite(cameraX)||!std::isfinite(cameraY))return result;
    const float dx=cameraX-site[0],dy=cameraY-site[1];
    // Native VCS 0x2E4D38: 400-unit radius, 200-unit detailed-model box.
    if(dx*dx+dy*dy>160000.0f)return result;
    static constexpr const char* names[]={"", "prot", "loan", "pros", "drug", "smug", "rob"};
    result.detailed=std::fabs(dx)<=200.0f&&std::fabs(dy)<=200.0f;
    const char variant=result.detailed?(damaged?'b':'a'):'c';
    result.building=std::string("eb_")+names[type]+"_"+std::to_string(scale)+"_"+variant;
    // Native VCS 0x2E52A8 adds this separate entity while state is 5.
    result.construction=state==5;
    return result;
}
