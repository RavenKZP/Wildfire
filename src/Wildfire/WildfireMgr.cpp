#include "Wildfire/Wildfire.h"
#include "Wildfire/Weather.h"

#include "CLibUtilsQTR/Tasker.hpp"

#ifndef NDEBUG
#include "CLibUtilsQTR/DrawDebug.hpp"
#endif

#include "SKSEMCP/SKSEMenuFramework.hpp"

#include "Settings.h"
#include "Utils.h"

WildfireMgr::WildfireMgr() { 
    auto* set = Settings::GetSingleton();
}

void WildfireMgr::OnGameLoad() {
    // Internal Grass Cleanup
    {
        std::lock_guard lock(cellStateMutex);
        for (auto& [cell, state] : cellStateMap) {
            if (cell && cell->IsAttached()) {
                SKSE::GetTaskInterface()->AddTask([this, cell]() { grassUpdater.RegenerateGrassInCell(cell); });
            }
        }
        cellStateMap.clear();
    }
    {
        std::lock_guard lock(grassEventMutex);
        grassEventQueue.clear();
    }

    // Internal Hazards Cleanup
    {
        // Nothing to do :)
    }

    // Internal Refs Cleanup
    {
        std::lock_guard lock(refStateMutex);
        refUpdater.ResetAllObjects(refStateMap);
        refStateMap.clear();
    }
    {
        std::lock_guard lock(refEventMutex);
        refEventQueue.clear();
    }

    // Set ignore Flags if running
    if (grassUpdater.IsRunning()) {
        ignore_next_grass_res = true;
    }
    if (hazardUpdater.IsRunning()) {
        ignore_next_hazard_res = true;
    }
    if (refUpdater.IsRunning()) {
        ignore_next_ref_res = true;
    }

    // Modules Cleanups
    hazardUpdater.OnGameLoad();
    grassUpdater.OnGameLoad();
    refUpdater.OnGameLoad();

    // Load the 
    //      cellStateMap
    //      burningRefs
    //      grassEventQueue
    //      refEventQueue
    // from SKSE's serialization system
}

void WildfireMgr::AddCellState(RE::TESObjectCELL* cell, CellState& state) {
    if (!cell) {
        return;
    }
    grassUpdater.CellLoaded(cell, state);
    std::lock_guard lock(cellStateMutex);
    cellStateMap.insert_or_assign(cell, std::move(state));
}

void WildfireMgr::AddRefState(RE::TESObjectREFR* ref, RefState& state) {
    if (!ref) {
        return;
    }
    refUpdater.RefLoaded(ref, state);
    std::lock_guard lock(refStateMutex);
    refStateMap.insert_or_assign(ref->GetHandle(), std::move(state));
}

void WildfireMgr::HazardLoad(RE::TESObjectREFR* a_ref) {
    return; // Temporary
    auto base = a_ref->GetBaseObject();
    if (auto modFile = base->GetFile()) {
        auto Filename = modFile->GetFilename();
        if (modFile->GetFilename() == "Wildfire.esp") {
            if (hazardUpdater.HazardLoaded(a_ref)) {
                // logger::debug("[Main][WildfireMgr] Fire or Smoke Hazard added to hazardUpdater");
                // it's Fire or Smoke, Hazard Updater will handle this
            } else {
                // it's not Fire or Smoke so it must be Dummy Fadeing Ref, no need to handle anyting
                // refUpdater.HazardLoaded(a_ref);
            }
        }
    }
}

void WildfireMgr::OnGameSave() {
    // Save the 
    //      cellStateMap 
    //      burningRefs 
    //      grassEventQueue 
    //      refEventQueue
    // to SKSE's serialization system

    // Important: effect shaders are saved by vanilla skyrim serialization system
}

CellStateMapType WildfireMgr::GetCellStateMapCopy() {
    std::lock_guard lock(cellStateMutex);
    return cellStateMap;
}

RefStateMapType WildfireMgr::GetRefStateMapCopy() {
    std::lock_guard lock(refStateMutex);
    return refStateMap;
}

bool WildfireMgr::GrassUpdate(float delta) {
    if (!grassUpdater.IsRunning()) {
        auto result = grassUpdater.GetResult();
        if (result) {
            if (ignore_next_grass_res) {
                ignore_next_grass_res = false;
            } else {
                std::lock_guard lock(cellStateMutex);
                for (auto& [cell, state] : *result) {
                    cellStateMap.insert_or_assign(cell, state);
                }
            }
            grassUpdater.ClearResult();            
        }
        GrassEventType eventsToProcess;
        {
            std::lock_guard lock(grassEventMutex);
            eventsToProcess.swap(grassEventQueue);
        }
        auto weatherData = Weather::GetWeatherData();

        std::lock_guard lock(cellStateMutex);
        return grassUpdater.StartUpdate(eventsToProcess, cellStateMap, weatherData, delta);
    } else {
        return false;  // Still running, can't start a new update
    }
}

void WildfireMgr::HazardUpdate() {
    if (!hazardUpdater.IsRunning()) {
        auto result = hazardUpdater.GetResult();
        if (result) {
            if (ignore_next_hazard_res) {
                ignore_next_hazard_res = false;
            } else {
                hazardUpdater.CopyResult();
            }
            hazardUpdater.ClearResult();
        }
        if (!hazardUpdater.StartUpdate(cellStateMap, refStateMap)) {
            logger::warn("[Main][WildfireMgr] Hazard update still running");
        }
    } else {
        logger::warn("[Main][WildfireMgr] Hazard update still running");
    }
}

void WildfireMgr::SetHazardsLifetine(float lifetime) { hazardUpdater.SetHazardsLifetine(lifetime); }

bool WildfireMgr::RefUpdate(float delta) {
    if (!refUpdater.IsRunning()) {
        auto result = refUpdater.GetResult();
        if (result) {
            if (ignore_next_ref_res) {
                ignore_next_ref_res = false;
            } else {
                std::lock_guard lock(refStateMutex);
                for (auto& [ref, state] : *result) {
                    refStateMap.insert_or_assign(ref, state);
                }
            }
            refUpdater.ClearResult();
        }
        RefEventType eventsToProcess;
        {
            std::lock_guard lock(refEventMutex);
            eventsToProcess.swap(refEventQueue);
        }
        auto weatherData = Weather::GetWeatherData();
        std::lock_guard lock(refStateMutex);
        return refUpdater.StartUpdate(eventsToProcess, refStateMap, weatherData, delta);
    } else {
        return false;  // Still running, can't start a new update
    }
}

static void LogPeriodicUpdateTime(std::chrono::steady_clock::time_point start) { 
    static auto* set = Settings::GetSingleton(); 
    if (set->EnableTimeLogging) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        logger::info("[Main][WildfireMgr] Whole WildfireMgr::PeriodicUpdate took {} ms", elapsed.count());
    }
}

static float elapsedMs(std::chrono::steady_clock::time_point frameStart) {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(now - frameStart).count();
}

void WildfireMgr::PeriodicUpdate(float delta) {
    static auto* set = Settings::GetSingleton();
    if (set->ModActive == false) {
        return;  // Mod is disabled, skip all processing
    }
    auto start = std::chrono::high_resolution_clock::now();

    static float grassTimer = 0.0f;
    static float hazardTimer = 0.0f;
    static float refTimer = 0.0f;

    grassTimer += delta;
    hazardTimer += delta;
    refTimer += delta;

#ifndef NDEBUG
    if (set->DebugMode && set->DrawWind) {
        static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
        static auto* player = RE::PlayerCharacter::GetSingleton();
        static float windTimer = 0.0f;
        windTimer += delta;
        if (windTimer >= set->DebugDrawDuration/1000.0f) {
            windTimer = 0.0f;
            auto weatherData = Weather::GetWeatherData();
            float windAngle = weatherData.windData.direction;
            auto p0 = player->GetPosition();

            float length = 500.0f;

            RE::NiPoint3 dir{std::cos(windAngle) * length, std::sin(windAngle) * length, 50.0f};
            RE::NiPoint3 p1 = p0 + dir;

            auto precipDir = RE::Sky::GetSingleton()->precip->GetDirection();
            precipDir.x *= length;
            precipDir.y *= length;
            precipDir.z += 50;
            RE::NiPoint3 p2 = p0 + precipDir;

            DebugAPI->DrawLineForMS(p0, p1, set->DebugDrawDuration, {0, 1, 0, 1}, 1);
            DebugAPI->DrawLineForMS(p0, p2, set->DebugDrawDuration, {1, 0, 0, 1}, 1);
        }
    }
#endif

    hazardUpdater.ManageHazards();
    refUpdater.TransformObjects(delta);

    if (cellStateMap.empty() && refStateMap.empty()) {
        hazardTimer = set->HazardPeriodicUpdateTime;
    } else if (hazardTimer >= set->HazardPeriodicUpdateTime) {
        hazardTimer = 0.0f;
        HazardUpdate();
        return LogPeriodicUpdateTime(start);
    }

    if (cellStateMap.empty() && grassEventQueue.empty()) {
        grassTimer = set->GrassPeriodicUpdateTime;
    } else if (grassTimer >= set->GrassPeriodicUpdateTime) {
        if (GrassUpdate(grassTimer)) {
            grassTimer = 0.0f;
            return LogPeriodicUpdateTime(start);
        } else {
            static int consecutiveRuns = 0;
            consecutiveRuns++;
            if (consecutiveRuns >= 5) {
                logger::error("[Main][WildfireMgr] Grass update has been running for {} consecutive updates.", consecutiveRuns);
            } else {
                logger::warn("[Main][WildfireMgr] Grass update still running");
            }
        }
        // If GrassUpdate returns false, we not reset the timer to try again next frame
        // This allows us to handle cases where we have more events than we can process in one update
    }

    if (refStateMap.empty() && refEventQueue.empty()) {
        refTimer = set->RefPeriodicUpdateTime;
    } else if (refTimer >= set->RefPeriodicUpdateTime) {
        if (RefUpdate(refTimer)) {
            refTimer = 0.0f;
            return LogPeriodicUpdateTime(start);
        } else {
            static int consecutiveRuns = 0;
            consecutiveRuns++;
            if (consecutiveRuns >= 5) {
                logger::error("[Main][WildfireMgr] References update has been running for {} consecutive updates.", consecutiveRuns);
            } else {
                logger::warn("[Main][WildfireMgr] References update still running");
            }
        }
        // If RefUpdate returns false, we not reset the timer to try again next frame
        // This allows us to handle cases where we have more events than we can process in one update
    }

    auto decalAndSmokeStart = std::chrono::high_resolution_clock::now();
    while (!decalAndSmokeQueue.empty()) {
        auto& pos = decalAndSmokeQueue.front();
        hazardUpdater.SpawnSmokeHazardAt(pos);

        if (decalUpdater.SpawnBurnDecalAtPosition(pos)) {
            // Successful decal spawn
        } else {
            if (set->EnableTimeLogging) {
                logger::warn("[Main][WildfireMgr] Failed to spawn decal");
            }
        }
        decalAndSmokeQueue.pop();
        if (elapsedMs(decalAndSmokeStart) > 1.0f) {
            logger::warn("[Main][WildfireMgr] Out of time budget while spawning Decal and Smoke");
            return LogPeriodicUpdateTime(start);
        }
    }

    if (grassUpdater.RegenerateGrass()) {
        return LogPeriodicUpdateTime(start);
    }

    return LogPeriodicUpdateTime(start);
}

void WildfireMgr::AddGrassEvent(const RE::NiPoint3& impactPos, float radius, float damage, ElementType type) {
    static auto* set = Settings::GetSingleton();

    if (type != ElementType::Shock && type != ElementType::Neutral &&
        type != ElementType::Unknown) {  // Shock can't affect grass
        {
            std::lock_guard lock(grassEventMutex);
            grassEventQueue.emplace_back(impactPos, radius, damage, type);
        }
        RE::TES::GetSingleton()->ForEachCellInRange(impactPos, radius, [this](RE::TESObjectCELL* cell) {
            if (cell && cell->IsExteriorCell()) {
                grassUpdater.GetOrCreateCellState(cellStateMap, cell);
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });

#ifndef NDEBUG
        if (set->DebugMode && set->DrawImpacts) {
            RE::NiColorA colour{0, 1, 0, 1};  // Green
            switch (type) {
                case ElementType::Fire:
                    colour = {1, 0, 0, 1};
                    break;
                case ElementType::Shock:
                    colour = {1, 0, 1, 1};
                    break;
                case ElementType::Cold:
                    colour = {0, 1, 1, 1};
                    break;
                case ElementType::Water:
                    colour = {0, 0, 1, 1};
                    break;
            }
            static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
            DebugAPI->DrawSphere(impactPos, radius, set->DebugDrawDuration, colour);
        }
#endif
    }
}

void WildfireMgr::AddRefEvent(RE::TESObjectREFR* ref, float damage, ElementType type) {
    if (!ref) {
        return;
    }
    if (ref->IsDisabled() || ref->IsDestroyed()) {
        return;
    }

    static auto* set = Settings::GetSingleton();

    if (ref && Utils::IsSupportedForm(ref)) {
        if (type != ElementType::Neutral && type != ElementType::Unknown) {
            auto refHandle = ref->GetHandle();
            {
                std::lock_guard lock(refEventMutex);
                refEventQueue.emplace_back(refHandle, damage, type);
            }
            refUpdater.GetOrCreateRefState(refStateMap, ref->GetHandle());
        }

#ifndef NDEBUG
        if (set->DebugMode && set->DrawImpacts) {
            RE::NiColorA colour{0, 1, 0, 1};  // Green
            switch (type) {
                case ElementType::Fire:
                    colour = {1, 0, 0, 1};
                    break;
                case ElementType::Shock:
                    colour = {1, 0, 1, 1};
                    break;
                case ElementType::Cold:
                    colour = {0, 1, 1, 1};
                    break;
                case ElementType::Water:
                    colour = {0, 0, 1, 1};
                    break;
            }
            static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
            RE::NiPoint3 p0 = Utils::GetPosition(ref);
            RE::NiPoint3 p1 = p0;
            p1.z += 50;
            DebugAPI->DrawLineForMS(p0, p1, set->DebugDrawDuration, colour, 10);
        }
#endif
    }
}

void WildfireMgr::SendHitEvent(RE::TESObjectREFR* ref) {
    if (ref) {
        // If is supported by OIF
        if (Utils::IsOIFSupported(ref)) {
            static RE::TESHitEvent event(ref, nullptr, 0x012FCD, 0x012FCF, RE::TESHitEvent::Flag::kNone);
            RE::ScriptEventSourceHolder::GetSingleton()->SendEvent(&event);
        }
        if (Utils::IsPapyrusSupported(ref)) {
            /*
            static RE::TESObjectREFR* aggressor = nullptr;
            static RE::TESForm* source = RE::TESForm::LookupByEditorID("Flames");
            static auto projectileForm = RE::TESForm::LookupByEditorID("FlamesProjectile");
            static RE::BGSProjectile* projectile = projectileForm->As<RE::BGSProjectile>();

            static bool powerAttack = false;
            static bool sneakAttack = false;
            static bool bashAttack = false;
            static bool hitBlocked = false;

            SendPapyrusEvent(ref, "OnHit", aggressor, source, projectile, false, false, false, false);
            */

        }
    }
}

void WildfireMgr::ReferenceBurned(const RE::NiPoint3& position) {
    decalAndSmokeQueue.push(position);

    //hazardUpdater.SpawnSmokeHazardAt(position);
    //decalUpdater.SpawnBurnDecalAtPosition(position);
}

void WildfireMgr::GrassBurned(const RE::NiPoint3& position) {
    decalAndSmokeQueue.push(position);

    //hazardUpdater.SpawnSmokeHazardAt(position);
    //decalUpdater.SpawnBurnDecalAtPosition(position);
}

void WildfireMgr::SetHazardsPCOnly(bool PCOnly) { hazardUpdater.SetHazardsPCOnly(PCOnly); }