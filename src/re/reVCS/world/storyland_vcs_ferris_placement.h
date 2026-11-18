#pragma once
#include "src/re/reLCS2/world/storyland_lcs_dynamic_placement.h"

inline bool storylandApplyVcsFerrisPlacements(StorylandArchiveBrowser& browser,
    const std::array<float,16>& matrix,std::string& error) {
    if(browser.hasNativeScenery()||browser.nativeWorldMasterOffset()!=UINT32_MAX)return true;
    const uint32_t master=browser.nativeWorldMasterOffset(true);
    if(master==UINT32_MAX)return true;
    const auto& masterBytes=browser.lvzBytes();
    const auto& instanceBytes=browser.imgBytes();
    if(uint64_t(master)+0x168u>masterBytes.size()) {
        error="Truncated VCS animated-placement pivot table.";return false;
    }
    const auto real=[](const std::vector<uint8_t>& bytes,size_t at) {
        const uint32_t bits=uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|
            uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;
        float value=0;std::memcpy(&value,&bits,4);return value;
    };
    for(size_t index=0;index<browser.placements().size();++index) {
        const auto& placement=browser.placements()[index];
        if(placement.iplId>=2)continue;
        if(placement.imgOffset>instanceBytes.size()||instanceBytes.size()-placement.imgOffset<0x50u) {
            error="Truncated VCS animated-placement matrix.";return false;
        }
        std::array<float,3> scale{},pivot{};
        for(size_t column=0;column<3;++column) {
            pivot[column]=real(masterBytes,size_t(master)+0x150+placement.iplId*12+column*4);
            if(!std::isfinite(pivot[column])) {
                error="Nonfinite VCS animated-placement pivot.";return false;
            }
            const size_t at=size_t(placement.imgOffset)+0x10+column*16;
            const float x=real(instanceBytes,at),y=real(instanceBytes,at+4),z=real(instanceBytes,at+8);
            scale[column]=std::sqrt(x*x+y*y+z*z);
            if(!std::isfinite(scale[column])||scale[column]<=0) {
                error="Invalid VCS animated-placement scale.";return false;
            }
        }
        if(!browser.setPlacementTransform(index,
            storylandLcsNormalizedPlacement(matrix,scale,pivot))) {
            error="VCS Ferris wheel render transform was rejected.";return false;
        }
    }
    error.clear();return true;
}
