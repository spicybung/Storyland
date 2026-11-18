#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct StorylandStatsMenuRow {
    const char* labelKey;
    const char* fallback;
    std::string value;
};

inline std::string storylandStatsMenuNumber(double value, unsigned decimals = 0) {
    if (!std::isfinite(value)) value = 0;
    if (std::fabs(value) < 0.000001) value = 0;
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(int(decimals)) << value;
    return stream.str();
}

template<class World>
std::vector<StorylandStatsMenuRow> storylandStatsMenuRows(const World& world, bool vcs) {
    const auto& stats = world.statistics;
    const auto number = [&](const char* key, double fallback = 0) {
        const auto found = stats.find(key);
        return found != stats.end() && std::isfinite(found->second) ? found->second : fallback;
    };
    std::vector<StorylandStatsMenuRow> rows;
    const auto add = [&](const char* key, const char* fallback, double value, unsigned decimals = 0) {
        rows.push_back({key, fallback, storylandStatsMenuNumber(value, decimals)});
    };
    const auto count = [&](const char* key, const char* fallback, const char* statistic) {
        const auto found=stats.find(statistic);
        if(found!=stats.end()&&std::isfinite(found->second))add(key,fallback,found->second);
    };
    const auto record = [&](const char* key, const char* fallback, const char* statistic, unsigned decimals = 0) {
        const auto found = stats.find(statistic);
        if (found != stats.end() && std::isfinite(found->second) && found->second != 0 &&
            found->second != double(INT32_MAX)) add(key, fallback, found->second, decimals);
    };
    const auto aliasedRecord = [&](const char* key, const char* fallback,
        const char* liveName, const char* legacyName, unsigned decimals = 0) {
        const auto live=stats.find(liveName);
        const auto legacy=stats.find(legacyName);
        const bool liveValid=live!=stats.end()&&std::isfinite(live->second);
        const bool legacyValid=legacy!=stats.end()&&std::isfinite(legacy->second);
        if(!liveValid&&!legacyValid)return;
        const double value=liveValid&&legacyValid?std::max(live->second,legacy->second):
            liveValid?live->second:legacy->second;
        if(value!=0&&value!=double(INT32_MAX))add(key,fallback,value,decimals);
    };
    // Key sequence follows the two PS2 statistics renderers; only rows with
    // an available Storyland counter or a defined native zero are included.
    const double made = number("ProgressMade", world.progressMade);
    const double total = number("TotalProgressInGame", world.progressTotal);
    add("PER_COM", "Percentage completed", total > 0 ? std::clamp(made * 100 / total, 0.0, 100.0) : 0, 2);
    add("NMISON", "Mission attempts", number("MissionsGiven", world.missionsGiven));
    count("DAYSPS", "Days passed in game", "DaysPassed");
    if (vcs) {
        record("ST_E_CD", "Total cash spent on Empire development", "EmpireMoneySpentOnDevelopment");
        record("ST_E_CR", "Total cash spent on Empire repair", "EmpireMoneySpentOnRepair");
        record("ST_E_BP", "Biggest payday", "EmpireLargestBuildingEarning");
        record("ST_E_TE", "Total Empire Earnings", "EmpireMoneyEarnedFromBuildings");
        const auto& sites=world.empireSitesOwned;
        if (sites[0]||sites[1]||sites[2]||sites[3]) {
            add("ST_E_OW", "Number of empire sites Vance owned", sites[0]);
            add("ST_E_OV", "Percentage empire sites Vance owned", sites[0]*100.0/30.0, 2);
            add("ST_E_OC", "Percentage empire sites Cholo owned", sites[1]*100.0/30.0, 2);
            add("ST_E_OS", "Percentage empire sites Sharks owned", sites[2]*100.0/30.0, 2);
            add("ST_E_OB", "Percentage empire sites Bikers owned", sites[3]*100.0/30.0, 2);
        }
        record("ATT_SUC", "Attacks successful", "EmpireAttacksSuccessful");
        record("DEF_SUC", "Sites successfully defended", "EmpireDefensesSuccessful");
    }
    count("FEST_RP", "Rampages passed", "NumberKillFrenziesPassed");
    count(vcs ? "BAL_BUR" : "PERPIC", vcs ? "Red Balloons burst" : "Hidden Packages found",
        vcs ? "BalloonsBurst" : "CollectedPackages");
    count("PE_WAST", "People you've wasted", "PeopleKilledByPlayer");
    if (vcs) count("NUM_NS", "Number of necks snapped", "NecksSnapped");
    count("PE_WSOT", "People wasted by others", "PeopleKilledByOthers");
    count("CAR_EXP", "Cars exploded", "CarsExploded");
    count("BOA_EXP", "Boats destroyed", "BoatsExploded");
    count("HEL_DST", vcs ? "Aircraft destroyed" : "Helicopters destroyed", "HelisDestroyed");
    count("TYREPOP", "Tires popped with gunfire", "TyresPopped");
    count("ST_STAR", "Total wanted stars attained", "WantedStarsAttained");
    count("ST_STGN", "Total wanted stars evaded", "WantedStarsEvaded");
    count("TM_BUST", "Times busted", "TimesArrested");
    count("TM_DED", vcs ? "Times you've been wasted" : "Hospital visits", "TimesDied");
    count("ST_HEAD", "Number of headshots", "HeadsPopped");
    count("KGS_EXP", "Kgs of explosives used", "KgsOfExplosivesUsed");
    count("BUL_FIR", "Bullets fired", "RoundsFiredByPlayer");
    count("BUL_HIT", "Bullets that hit", "BulletsThatHit");
    const auto shots=stats.find("RoundsFiredByPlayer");
    const auto hits=stats.find("BulletsThatHit");
    if(shots!=stats.end()&&hits!=stats.end()&&std::isfinite(shots->second)&&
       std::isfinite(hits->second)&&shots->second>0)
        add("ACCURA", "Accuracy", hits->second * 100.0 / shots->second, 2);
    const auto distance = [&](const char* key, const char* fallback, const char* stat) {
        const auto found=stats.find(stat);
        if(found!=stats.end()&&std::isfinite(found->second))add(key, fallback, found->second / 1609.344, 2);
    };
    distance("FEST_DF", "Distance travelled on foot (miles)", "DistanceTravelledOnFoot");
    if (vcs) distance("DISTSWI", "Distance travelled by swimming (miles)", "DistanceTravelledBySwimming");
    distance("FEST_DC", "Distance travelled by car (miles)", "DistanceTravelledByCar");
    distance("DISTBIK", "Distance travelled by bike (miles)", "DistanceTravelledByBike");
    distance("DISTBOA", "Distance travelled by boat (miles)", "DistanceTravelledByBoat");
    record("MXCARDM", "Maximum jump distance (m)", "MaximumJumpDistance", 2);
    record("MXCARJM", "Maximum jump height (m)", "MaximumJumpHeight", 2);
    record("MXFLIP", "Maximum jump flips", "MaximumJumpFlips");
    record("MXJUMP", "Maximum jump rotation", "MaximumJumpSpins");
    add("NOUNIF", "Unique jumps completed", number("NumberOfUniqueJumpsFound", world.uniqueJumpsFound));
    aliasedRecord("ST_WHEE", "Longest wheelie time (secs)", "LongestWheelieTime", "LongestWheelie");
    aliasedRecord("ST_WHED", "Longest wheelie distance (m)", "LongestWheelieDistance", "LongestWheelieDist", 2);
    aliasedRecord("ST_STOP", "Longest stoppie time (secs)", "LongestStoppieTime", "LongestStoppie");
    aliasedRecord("ST_STOD", "Longest stoppie distance (m)", "LongestStoppieDistance", "LongestStoppieDist", 2);
    record("ST_FAPD", "Longest face plant distance (m)", "LongestFacePlantDistance", 2);
    aliasedRecord("ST_2WHE", "Longest 2 wheels time (secs)", "Longest2WheelTime", "Longest2Wheel");
    aliasedRecord("ST_2WHD", "Longest 2 wheels distance (m)", "Longest2WheelDistance", "Longest2WheelDist", 2);
    count("FEST_CC", "Vigilante justice served", "CriminalsCaught");
    count("FEST_HV", "Highest Vigilante level", "HighestLevelVigilanteMission");
    count("PASDRO", "Passengers dropped off", "PassengersDroppedOffWithTaxi");
    count("MONTAX", "Cash made in taxi", "MoneyMadeWithTaxi");
    count("FEST_LS", "People saved in an Ambulance", "LivesSavedWithAmbulance");
    count("FEST_HA", "Highest Paramedic level", "HighestLevelAmbulanceMission");
    count("FEST_FE", "Total fires extinguished", "FiresExtinguished");
    count("FIRELVL", "Highest Firefighter level", "HighestLevelFireMission");
    if(vcs) {
        record("ST_CRA", "Most damage caused on Crash", "CrashMissionMostDamage");
        record("ST_CRA2", "Most damage caused on Rush", "RushMostDamage");
        record("FEST_SH", "Total Shoplifters apprehended", "ShopliftersApprehended");
        record("ST_SLGS", "Greatest value of goods saved on Crimewave", "CrimewaveValueOfGoodsSaved");
        record("ST_SLHL", "Highest Crimewave level", "CrimewaveHighestLevel");
        record("ST_ARHL", "Highest Air Rescue level", "AirAmbulanceHighestLevel");
        record("ST_HTHL", "Highest Vice Sights level", "ViceSightsHighestLevel");
        record("ST_HTSW", "Sights viewed on Vice Sights", "ViceSightsViewed");
        record("ST_FHHL", "Highest Fire Copter level", "FireCopterHighestLevel");
        record("ST_BPHL", "Highest Beach Patrol level", "BeachPatrolHighestLevel");
        record("ST_BSDR", "Best score at the driving range", "DrivingRangeBestScore");
        record("ST_HDDR", "Furthest drive on the range (m)", "DrivingRangeFurthestDrive", 2);
        record("ST_BSSR", "Best score on Phil's Shooting Range", "ShootingRangeBestScore");
        record("ST_BSSA", "Best accuracy on Phil's Shooting Range", "ShootingRangeBestAccuracy");
    }
    record("ST_PHOT", "Photos taken", "PhotosTaken");
    if(!vcs) record("ST_PIZZ", "Pizzas delivered", "PizzasDelivered");
    record("ST_DRWN", "Fish fed", "TimesDrowned");
    record("SEAGULL", "Seagulls sniped", "SeagullsKilled");
    record("SPRAYIN", "Sprayings", "Sprayings");
    record("ST_WEAP", "Weapon budget", "WeaponBudget", 2);
    record("ST_AUTO", "Auto repair and painting budget", "AutoPaintingBudget", 2);
    record("ST_DAMA", "Property destroyed", "PropertyDestroyed");
    if(vcs) record("TMSOUT", "Outfit changes", "TimesOutfitChanged");
    return rows;
}
