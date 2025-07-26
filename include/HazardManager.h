#pragma once
#include "WildfireMgr.h"

#include "ClibUtil/singleton.hpp"

class HazardMgr : public clib_util::singleton::ISingleton<HazardMgr> {
public:
    void PeriodicUpdate(float delta);
    void InitializeHazards();
    
    RE::BGSHazard* FireLgShortHazard;
    RE::BGSHazard* FireSmLongHazard;
    RE::BGSHazard* FireSmShortHazard;
    RE::BGSHazard* TrapOilHazard01;

    RE::BGSHazard* FireDragonHazard;

private:
    void SpawnFxAtVertex(const FireVertex& vertex, float lifetime, const char* fxModel);
    void SpawnHazardAtVertex(const FireVertex& vertex, RE::BGSHazard* hazardForm, float baseScale, float lifetime);

};