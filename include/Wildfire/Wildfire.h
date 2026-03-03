#pragma once

#include "REX/REX.h"

#include "Types.h"
#include "Wildfire/GrassUpdater.h" 
#include "Wildfire/HazardUpdater.h"
#include "Wildfire/ReferenceUpdater.h"

#include <shared_mutex>

class WildfireMgr : public REX::Singleton<WildfireMgr> {
public:

    void PeriodicUpdate(float delta);

    bool GrassUpdate(float delta);
    void HazardUpdate();
    bool RefUpdate(float delta);

    void ProcessCone(RE::Projectile* a_proj);
    void ProcessImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& pos);
    void ProcessExplosion(RE::Explosion* exp);

    void AddGrassEvent(const RE::NiPoint3& impactPos, float radius, float damage, ImpactType type);
    void AddRefEvent(RE::TESObjectREFR* ref, float damage, ImpactType type);

    void OnGameLoad();
    void OnGameSave();

    // For MCP debug
    CellStateMapType GetCellStateMapCopy();

private:
    // Queue of fire events
    std::shared_mutex grassEventMutex;
    GrassEventType grassEventQueue;

    // Map of cells to their fire state
    std::shared_mutex cellStateMutex;
    CellStateMapType cellStateMap;

    // Queue of ref events
    std::shared_mutex refEventMutex;
    RefEventType refEventQueue;

    // List of burning references and their state
    std::shared_mutex refStateMutex;
    RefStateMapType refStateMap;

    GrassUpdater grassUpdater;
    HazardUpdater hazardUpdater;
    ReferenceUpdater refUpdater;

    bool ignore_next_grass_res = false;
    bool ignore_next_hazard_res = false;
    bool ignore_next_ref_res = false;
};