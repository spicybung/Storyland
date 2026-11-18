#pragma once
#include "src/storyland/storyland_save.h"
#include "src/storyland/storyland_scm.h"

// reLCS PC GenericSave profile: simple variables/scripts, garages, player info, stats.
// Imports verified progression, statistics and garage records. Running entity pools are not interchangeable.
struct StorylandRelcsProgress {
    std::vector<StorylandMissionEnvironment::Garage> garages;
    std::vector<storyland_save::Vehicle> vehicles;
    std::map<size_t,int32_t> globals;
    std::map<std::string,double> statistics;
    std::string lastMissionPassed;
    int32_t money=0;
    unsigned hour=0,minute=0;
};
inline void storylandDecodeRelcsStatistics(const std::vector<uint8_t>& block,StorylandRelcsProgress& result) {
    // Exact packed CStats::SaveStats order from the reLCS PC profile.
    struct Field {const char* name;size_t count;char type;};
    static constexpr Field fields[]={
        {"PeopleKilledByPlayer",1,'i'},
        {"PeopleKilledByOthers",1,'i'},
        {"CarsExploded",1,'i'},
        {"BoatsExploded",1,'i'},
        {"TyresPopped",1,'i'},
        {"RoundsFiredByPlayer",1,'i'},
        {"PedsKilledOfThisType",23,'i'},
        {"HelisDestroyed",1,'i'},
        {"ProgressMade",1,'f'},
        {"TotalProgressInGame",1,'f'},
        {"KgsOfExplosivesUsed",1,'i'},
        {"BulletsThatHit",1,'i'},
        {"HeadsPopped",1,'i'},
        {"WantedStarsAttained",1,'i'},
        {"WantedStarsEvaded",1,'i'},
        {"TimesArrested",1,'i'},
        {"TimesDied",1,'i'},
        {"DaysPassed",1,'i'},
        {"SafeHouseVisits",1,'i'},
        {"Sprayings",1,'i'},
        {"MaximumJumpDistance",1,'f'},
        {"MaximumJumpHeight",1,'f'},
        {"MaximumJumpFlips",1,'i'},
        {"MaximumJumpSpins",1,'i'},
        {"BestStuntJump",1,'i'},
        {"NumberOfUniqueJumpsFound",1,'i'},
        {"TotalNumberOfUniqueJumps",1,'i'},
        {"MissionsGiven",1,'i'},
        {"PassengersDroppedOffWithTaxi",1,'i'},
        {"MoneyMadeWithTaxi",1,'i'},
        {"IndustrialPassed",1,'i'},
        {"CommercialPassed",1,'i'},
        {"SuburbanPassed",1,'i'},
        {"PamphletMissionPassed",1,'i'},
        {"NoMoreHurricanes",1,'i'},
        {"DistanceTravelledOnFoot",1,'f'},
        {"DistanceTravelledByCar",1,'f'},
        {"DistanceTravelledByBike",1,'f'},
        {"DistanceTravelledByBoat",1,'f'},
        {"DistanceTravelledByGolfCart",1,'f'},
        {"DistanceTravelledByHelicoptor",1,'f'},
        {"DistanceTravelledByPlane",1,'f'},
        {"LivesSavedWithAmbulance",1,'i'},
        {"CriminalsCaught",1,'i'},
        {"FiresExtinguished",1,'i'},
        {"HighestLevelVigilanteMission",1,'i'},
        {"HighestLevelAmbulanceMission",1,'i'},
        {"HighestLevelFireMission",1,'i'},
        {"PhotosTaken",1,'i'},
        {"NumberKillFrenziesPassed",1,'i'},
        {"TotalNumberKillFrenzies",1,'i'},
        {"TotalNumberMissions",1,'i'},
        {"FlightTime",1,'i'},
        {"TimesDrowned",1,'i'},
        {"SeagullsKilled",1,'i'},
        {"WeaponBudget",1,'f'},
        {"FashionBudget",1,'f'},
        {"LoanSharks",1,'f'},
        {"StoresKnockedOff",1,'f'},
        {"MovieStunts",1,'f'},
        {"Assassinations",1,'f'},
        {"PizzasDelivered",1,'f'},
        {"GarbagePickups",1,'f'},
        {"IceCreamSold",1,'f'},
        {"TopShootingRangeScore",1,'f'},
        {"ShootingRank",1,'f'},
        {"LongestWheelie",1,'i'},
        {"LongestStoppie",1,'i'},
        {"Longest2Wheel",1,'i'},
        {"LongestWheelieDist",1,'f'},
        {"LongestStoppieDist",1,'f'},
        {"Longest2WheelDist",1,'f'},
        {"PropertyBudget",1,'f'},
        {"AutoPaintingBudget",1,'f'},
        {"PropertyDestroyed",1,'i'},
        {"NumPropertyOwned",1,'i'},
        {"BloodRingKills",1,'i'},
        {"BloodRingTime",1,'i'},
        {"PropertyOwned",15,'b'},
        {"HighestChaseValue",1,'f'},
        {"FastestTimes",23,'i'},
        {"HighestScores",5,'i'},
        {"BestPositions",1,'i'},
        {"KillsSinceLastCheckpoint",1,'i'},
        {"TotalLegitimateKills",1,'i'},
        {"LastMissionPassedName",8,'s'},
        {"CheatedCount",1,'i'},
        {"FavoriteRadioStationList",11,'f'},
    };
    if(block.size()==4)return; // Older progression-only exports contain no statistics.
    if(block.size()!=603&&block.size()!=604)throw std::runtime_error("Unsupported reLCS statistics layout.");
    size_t cursor=4;
    for(const auto& field:fields) {
        if(field.type=='s') {
            for(size_t i=0;i<field.count;++i) {
                const auto ch=block[cursor+i];if(!ch)break;
                if(ch<32||ch>126)throw std::runtime_error("Invalid reLCS mission label.");
                result.lastMissionPassed.push_back(char(ch));
            }
            cursor+=field.count;continue;
        }
        for(size_t index=0;index<field.count;++index) {
            double value=0;
            if(field.type=='b') {if(block[cursor]>1)throw std::runtime_error("Invalid reLCS statistic flag.");value=block[cursor++];}
            else {
                const uint32_t bits=uint32_t(block[cursor])|uint32_t(block[cursor+1])<<8|
                    uint32_t(block[cursor+2])<<16|uint32_t(block[cursor+3])<<24;cursor+=4;
                if(field.type=='f') {float decoded;std::memcpy(&decoded,&bits,4);value=decoded;}
                else value=int32_t(bits);
                if(!std::isfinite(value))throw std::runtime_error("Invalid reLCS numeric statistic.");
            }
            std::string key=field.name;if(field.count>1)key+="["+std::to_string(index)+"]";
            result.statistics.emplace(std::move(key),value);
        }
    }
}
inline void storylandDecodeRelcsGarages(const std::vector<uint8_t>& block,StorylandRelcsProgress& result) {
    if(block.size()==4)return;
    // Validated against the 8,816-byte garage blocks in both supplied PC saves:
    // 44-byte header, 48 stored cars (44 bytes each), 32 garage records (208 bytes).
    if(block.size()!=8816)throw std::runtime_error("Unsupported reLCS garage layout.");
    const auto word=[&](size_t p){return uint32_t(block[p])|uint32_t(block[p+1])<<8|uint32_t(block[p+2])<<16|uint32_t(block[p+3])<<24;};
    const auto real=[&](size_t p){const auto bits=word(p);float f;std::memcpy(&f,&bits,4);
        if(!std::isfinite(f))throw std::runtime_error("Invalid garage coordinate.");return f;};
    const uint32_t count=word(4);if(count>32)throw std::runtime_error("Invalid reLCS garage count.");
    std::map<int,int> hideouts;
    for(uint32_t i=0;i<count;++i) {
        const size_t p=2160+i*208;
        StorylandMissionEnvironment::Garage garage;
        garage.handle=int32_t(i);garage.type=block[p];garage.maximumCars=block[p+2];
        if(garage.type>32||garage.maximumCars>4||block[p+205]>1)throw std::runtime_error("Invalid reLCS garage descriptor.");
        garage.locked=block[p+205]!=0;garage.swingDoor=block[p+25]!=0;
        const float x=real(p+28),y=real(p+32),length1=real(p+76),length2=real(p+80);
        garage.bounds={x,y,real(p+36),x+real(p+40)*length1,y+real(p+44)*length1,
            x+real(p+48)*length2,y+real(p+52)*length2,real(p+56)};
        const int type=garage.type;
        if(type>=16&&type<=18)hideouts[type-16]=garage.handle;
        else if(type>=24&&type<=32)hideouts[type-21]=garage.handle;
        result.garages.push_back(garage);
    }
    for(size_t i=0;i<48;++i) {
        const size_t p=48+i*44;const int32_t model=int32_t(word(p));if(!model)continue;
        if(model<0)throw std::runtime_error("Invalid stored vehicle model.");
        if(word(p+32)!=0||block[p+39]!=0||block[p+40]!=0||block[p+41]!=0||std::fabs(real(p+28)-1.0f)>.0001f)
            throw std::runtime_error("This stored car has proof/variation/bomb or traction settings not yet supported by the importer.");
        const auto garage=hideouts.find(int(i%12));
        if(garage==hideouts.end())throw std::runtime_error("Stored car references a missing hideout garage.");
        storyland_save::Vehicle car;car.model=uint32_t(model);car.garage=garage->second;car.actor.vehicle=true;
        car.actor.position={real(p+4),real(p+8),real(p+12)};
        car.actor.yaw=std::atan2(-real(p+16),real(p+20))*57.2957795131f+90;
        car.bodyPitch=std::atan2(real(p+24),std::hypot(real(p+16),real(p+20)))*57.2957795131f;
        car.primaryColor=int8_t(block[p+36]);car.secondaryColor=int8_t(block[p+37]);car.radioStation=int8_t(block[p+38]);
        result.vehicles.push_back(car);
    }
}
inline bool storylandDecodeRelcsProgress(const std::vector<uint8_t>& bytes,
    const std::vector<uint8_t>& scm,StorylandRelcsProgress& result,std::string& error) {
    try {
        const auto word=[](const std::vector<uint8_t>& data,size_t offset) {
            if(offset>data.size()||data.size()-offset<4)throw std::runtime_error("Truncated reLCS save field.");
            return uint32_t(data[offset])|uint32_t(data[offset+1])<<8|
                uint32_t(data[offset+2])<<16|uint32_t(data[offset+3])<<24;
        };
        if(bytes.size()<128||bytes.size()>storyland_save::MaximumBytes)throw std::runtime_error("Invalid reLCS save size.");
        uint32_t checksum=0;
        for(size_t index=0;index<bytes.size()-4;++index)checksum+=bytes[index];
        if(checksum!=word(bytes,bytes.size()-4))throw std::runtime_error("reLCS save checksum failed.");
        std::vector<std::vector<uint8_t>> blocks;
        size_t cursor=0;
        while(cursor<bytes.size()-4) {
            if(blocks.size()>=40)throw std::runtime_error("Too many reLCS save blocks.");
            if(bytes.size()-4-cursor<4)throw std::runtime_error("Truncated reLCS block header.");
            const uint32_t length=word(bytes,cursor);cursor+=4;
            const uint64_t padded=(uint64_t(length)+3)&~uint64_t(3);
            if(padded>bytes.size()-4-cursor)throw std::runtime_error("Invalid reLCS block length.");
            blocks.emplace_back(bytes.begin()+cursor,bytes.begin()+cursor+length);
            cursor+=size_t(padded);
        }
        if(cursor!=bytes.size()-4||blocks.size()<4)throw std::runtime_error("Incomplete reLCS save.");
        size_t playerBlock=2,statsBlock=3,firstPadding=4;
        // Earlier reLCS builds wrote the VC-derived 22-block container. Only import
        // the four compatible LCS subsystems; never apply its foreign entity pools.
        if(blocks.size()>=22&&blocks[9].size()>=8&&word(blocks[9],4)==0x00545352&&
            blocks[10].size()>=8&&word(blocks[10],4)==0x00524452&&
            blocks[11].size()>=8&&word(blocks[11],4)==0x00534e5a&&
            blocks[12].size()>=8&&word(blocks[12],4)==0x00474e47&&
            blocks[13].size()>=8&&word(blocks[13],4)==0x004e4743&&
            blocks[21].size()>=8&&word(blocks[21],4)==0x00505450) {
            playerBlock=17;statsBlock=18;firstPadding=22;
        }
        for(size_t index=firstPadding;index<blocks.size();++index)
            if(std::any_of(blocks[index].begin(),blocks[index].end(),[](uint8_t value){return value!=0;}))
                throw std::runtime_error("This reLCS build uses an unsupported save-pool layout.");
        if(scm.size()<32||scm[8]!=2||scm[9]!=0||scm[10]!=6)
            throw std::runtime_error("Load the matching LCS MAIN.SCM before importing.");
        const uint32_t variableBytes=word(scm,11);
        const uint64_t table=uint64_t(variableBytes)+16;
        if(table>scm.size()-4)throw std::runtime_error("Invalid LCS persistent-variable table.");
        const uint32_t variableCount=word(scm,size_t(table));
        if(variableCount>6656||uint64_t(variableCount)*2>scm.size()-table-4)
            throw std::runtime_error("Invalid LCS persistent-variable count.");
        const auto& first=blocks.front();
        size_t script=SIZE_MAX;
        for(size_t offset=0xC0;offset<=0x180&&offset+12<=first.size();offset+=4) {
            if(word(first,offset+4)!=0x00524353)continue;
            const uint32_t size=word(first,offset);
            if(size<12||size>first.size()-offset-4||word(first,offset+8)!=size-8)continue;
            if(script!=SIZE_MAX)throw std::runtime_error("Ambiguous reLCS script block.");
            script=offset+4;
        }
        if(script==SIZE_MAX||word(first,script+8)!=variableBytes||variableBytes<8||
            uint64_t(script)+12+variableBytes>first.size())
            throw std::runtime_error("Save does not match the loaded LCS script variable space.");
        const size_t variables=script+12;
        if(!std::equal(scm.begin()+8,scm.begin()+16,first.begin()+variables))
            throw std::runtime_error("reLCS save and MAIN.SCM headers do not match.");
        const size_t metadata=variables+variableBytes;
        const uint32_t metadataSize=word(first,metadata);
        const uint64_t programSizes=uint64_t(metadata)+metadataSize;
        const uint64_t scriptEnd=uint64_t(script)+8+word(first,script+4);
        if(metadataSize<20||metadataSize>8192||programSizes+16>scriptEnd||scriptEnd>first.size()||
            word(first,size_t(programSizes))!=word(scm,0)||word(first,size_t(programSizes)+4)!=word(scm,4))
            throw std::runtime_error("reLCS script sizes do not match the loaded MAIN.SCM.");
        StorylandRelcsProgress candidate;
        for(uint32_t index=0;index<variableCount;++index) {
            const size_t entry=size_t(table)+4+index*2;
            const uint32_t offset=uint32_t(scm[entry])|uint32_t(scm[entry+1])<<8;
            if(offset<8||offset%4||offset>variableBytes-4||offset/4>=6656)
                throw std::runtime_error("Invalid persistent variable in LCS MAIN.SCM.");
            candidate.globals[offset/4]=int32_t(word(first,variables+offset));
        }
        const auto& player=blocks[playerBlock];
        const auto& stats=blocks[statsBlock];
        if(player.size()<44||word(player,0)!=player.size()-4||
            blocks[1].size()<4||word(blocks[1],0)!=blocks[1].size()-4||
            stats.size()<4||word(stats,0)>stats.size()-4||stats.size()-4-word(stats,0)>3)
            throw std::runtime_error("Unsupported reLCS player/garage/stat block profile.");
        storylandDecodeRelcsStatistics(stats,candidate);
        storylandDecodeRelcsGarages(blocks[1],candidate);
        candidate.money=int32_t(word(player,4));
        candidate.hour=first[92];candidate.minute=first[96];
        if(candidate.hour>23||candidate.minute>59||candidate.money<0)
            throw std::runtime_error("Invalid reLCS player information.");
        result=std::move(candidate);return true;
    }catch(const std::exception& exception){error=exception.what();return false;}
}
inline bool storylandReadRelcsProgress(const std::filesystem::path& path,
    const StorylandScmFile& scm,StorylandRelcsProgress& result,std::string& error) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream){error="Cannot open the reLCS save.";return false;}
    const auto length=stream.tellg();
    if(length<0||uint64_t(length)>storyland_save::MaximumBytes){error="Invalid reLCS save size.";return false;}
    std::vector<uint8_t> bytes(static_cast<size_t>(length));stream.seekg(0);
    if(!stream.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()))) {
        error="Cannot read the reLCS save.";return false;
    }
    return storylandDecodeRelcsProgress(bytes,scm.rawBytes(),result,error);
}
