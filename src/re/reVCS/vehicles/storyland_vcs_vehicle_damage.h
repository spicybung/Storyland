#pragma once
#include <cstdint>

inline bool storylandVcsVehicleWeaponDamageAdmitted(int32_t weapon,int32_t status,float health) {
    if(status!=0||!(health<250.0f))return true;
    // CVehicle::InflictDamage, 003E8240..003E830C: test the pre-hit health.
    switch(weapon) {
    case 12:case 17:case 18:
    case 19:case 20:case 21:case 22:case 23:case 24:case 25:
    case 26:case 27:case 28:case 29:case 30:case 31:case 32:
    case 34:case 35:case 37:case 44:case 45:
        return false;
    default:
        return true;
    }
}
