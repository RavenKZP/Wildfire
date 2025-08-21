#pragma once
#include "Types.h"

#include "ClibUtil/singleton.hpp"

#include <shared_mutex>

class HazardMgr : public clib_util::singleton::ISingleton<HazardMgr> {
public:
    void InitializeHazards();

    void PeriodicUpdate(float delta);
    
    RE::BGSHazard* FireLgShortHazard;
    RE::BGSHazard* FireSmLongHazard;
    RE::BGSHazard* FireSmShortHazard;
    RE::BGSHazard* TrapOilHazard01;

    RE::BGSHazard* FireDragonHazard;

    void CreateBurningVertex(const FireVertex& vertex, float lifetime);
    void SpawnHazardAt(RE::NiPoint3 pos, RE::BGSHazard* hazardForm, float lifetime);

private:

    std::shared_mutex burnGridMutex;
    std::unordered_map<HazardGridCoord, float> burnGrid;

};