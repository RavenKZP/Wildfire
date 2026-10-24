#pragma once

#include "REX/REX.h"

#include "Types.h"
#include "Wildfire/DecalUpdater.h"
#include "Wildfire/GrassUpdater.h" 
#include "Wildfire/HazardUpdater.h"
#include "Wildfire/ReferenceUpdater.h"

#include <mutex>

class WildfireMgr : public REX::Singleton<WildfireMgr> {
public:
    WildfireMgr();

    void PeriodicUpdate(float delta);

    void HazardLoad(RE::TESObjectREFR* a_ref);

    bool GrassUpdate(float delta);
    void HazardUpdate();
    bool RefUpdate(float delta);

    void SetHazardsLifetine(float lifetime);

    void ProcessCone(RE::Projectile* a_projectile);
    void RemoveCone(RE::Projectile* a_projectile);
    void ProcessImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& pos);
    void RemoveProjectile(RE::Projectile* a_projectile);
    void ProcessExplosion(RE::Explosion* exp);

    void AddGrassEvent(const RE::NiPoint3& impactPos, float radius, float damage, ElementType type);
    void AddRefEvent(RE::TESObjectREFR* ref, float damage, ElementType type);
    void SendHitEvent(RE::TESObjectREFR* ref);

    void ReferenceBurned(const RE::NiPoint3& position);
    void GrassBurned(const RE::NiPoint3& position);

    void SetHazardsPCOnly(bool PCOnly);

    //Caled BEFORE Serialization Load
    void OnGameLoad();
    void AddCellState(RE::TESObjectCELL* cell, CellState& state);
    void AddRefState(RE::TESObjectREFR* ref, RefState& state);

    void OnGameSave();


    CellStateMapType GetCellStateMapCopy();
    RefStateMapType GetRefStateMapCopy();

private:
    // Queue of fire events
    mutable std::mutex grassEventMutex;
    GrassEventType grassEventQueue;

    // Map of cells to their fire state
    mutable std::mutex cellStateMutex;
    CellStateMapType cellStateMap;

    // Queue of ref events
    mutable std::mutex refEventMutex;
    RefEventType refEventQueue;

    // List of burning references and their state
    mutable std::mutex refStateMutex;
    RefStateMapType refStateMap;

    std::queue<RE::NiPoint3> decalAndSmokeQueue;

    GrassUpdater grassUpdater;
    HazardUpdater hazardUpdater;
    ReferenceUpdater refUpdater;
    DecalUpdater decalUpdater;

    bool ignore_next_grass_res = false;
    bool ignore_next_hazard_res = false;
    bool ignore_next_ref_res = false;
};