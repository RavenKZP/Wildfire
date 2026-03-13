#include "Wildfire/Wildfire.h"
#include "Wildfire/Weather.h"

#include "CLibUtilsQTR/Tasker.hpp"
// To be removed in release builds
#include "CLibUtilsQTR/DrawDebug.hpp"
#include "SKSEMCP/SKSEMenuFramework.hpp"

#include "Settings.h"
#include "Utils.h"

WildfireMgr::WildfireMgr() {
    auto ref = RE::TESDataHandler::GetSingleton()->LookupForm(0x097, "Wildfire.esp");
    if (!ref) {
        logger::error("Wildfire_SpiritRef not found. Please make sure Wildfire.esp is active");
    }
    auto actor = ref ? ref->As<RE::Actor>() : nullptr;
    Wildfire_Spirit = actor;

    if (Wildfire_Spirit) {
        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(0x012FD0);  // Firebolt
        RE::ActorEquipManager::GetSingleton()->EquipSpell(Wildfire_Spirit, spell);

    } else {
        logger::error("Wildfire Spirit not found. Please make sure Wildfire.esp is active");
        RE::DebugMessageBox("Wildfire Error: Wildfire Spirit not found. Please make sure Wildfire.esp is active");
    }
}

void WildfireMgr::OnGameLoad() {
    // Internal Grass Cleanup
    {
        std::unique_lock lock(cellStateMutex);
        for (auto& [cell, state] : cellStateMap) {
            if (cell && cell->IsAttached()) {
                if (auto* cellLand = cell->GetLandscape()) {
                    if (auto* loadedData = cellLand->loadedData) {
                        if (auto* colors = loadedData->colors) {
                            std::memcpy(colors, state.originalColors, sizeof(state.originalColors));
                            SKSE::GetTaskInterface()->AddTask([this, cell]() { grassUpdater.RegenerateGrassInCell(cell); });
                        }
                    }
                }
            }
        }
        cellStateMap.clear();
    }
    {
        std::unique_lock lock(grassEventMutex);
        grassEventQueue.clear();
    }

    // Internal Hazards Cleanup
    {
        // Nothing to do :)
    }

    // Internal Refs Cleanup
    {
        std::unique_lock lock(refStateMutex);
        for (auto& [refHandle, state] : refStateMap) {
            auto ref = refHandle.get().get();
            if (ref) {
                if (auto a3D = ref->Get3D()) {
                    a3D->CullGeometry(false);
                }
            }
        }
        refStateMap.clear();
    }
    {
        std::unique_lock lock(refEventMutex);
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

    // External Cleanups
    hazardUpdater.OnGameLoad();
    grassUpdater.OnGameLoad();
    refUpdater.OnGameLoad();

    // Load the 
    //      cellStateMap
    //      burningRefs
    //      grassEventQueue
    //      refEventQueue
    // from SKSE's serialization system

    auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(0x012FD0);  // Firebolt
    RE::ActorEquipManager::GetSingleton()->EquipSpell(Wildfire_Spirit, spell);
}

void WildfireMgr::OnGameSave() {
    // Save the 
    //      cellStateMap 
    //      burningRefs 
    //      grassEventQueue 
    //      refEventQueue
    // to SKSE's serialization system

    // Important effect shaders are saved by vanilla skyrim serialization system
}

CellStateMapType WildfireMgr::GetCellStateMapCopy() {
    std::shared_lock lock(cellStateMutex);
    return cellStateMap;
}

RefStateMapType WildfireMgr::GetRefStateMapCopy() {
    std::shared_lock lock(refStateMutex);
    return refStateMap;
}

bool WildfireMgr::GrassUpdate(float delta) {
    if (!grassUpdater.IsRunning()) {
        auto result = grassUpdater.GetResult();
        if (result) {
            if (ignore_next_grass_res) {
                ignore_next_grass_res = false;
            } else {
                std::unique_lock lock(cellStateMutex);
                for (auto& [cell, state] : *result) {
                    cellStateMap.insert_or_assign(cell, state);
                }
            }
            grassUpdater.ClearResult();            
        }
        GrassEventType eventsToProcess;
        {
            std::unique_lock lock(grassEventMutex);
            eventsToProcess.swap(grassEventQueue);
        }
        auto weatherData = Weather::GetWeatherData();

        std::unique_lock lock(cellStateMutex);
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
            logger::warn("Hazard update still running, spawning on previous positions");
        }
    } else {
        logger::warn("Hazard update still running, spawning on previous positions");
    }
    hazardUpdater.SpawnHazards();
}

void WildfireMgr::SetHazardsLifetine(float lifetime) { hazardUpdater.SetHazardsLifetine(lifetime); }

bool WildfireMgr::RefUpdate(float delta) {
    if (!refUpdater.IsRunning()) {
        auto result = refUpdater.GetResult();
        if (result) {
            if (ignore_next_ref_res) {
                ignore_next_ref_res = false;
            } else {
                std::unique_lock lock(refStateMutex);
                for (auto& [ref, state] : *result) {
                    refStateMap.insert_or_assign(ref, state);
                }
            }
            refUpdater.ClearResult();
        }
        RefEventType eventsToProcess;
        {
            std::unique_lock lock(refEventMutex);
            eventsToProcess.swap(refEventQueue);
        }
        auto weatherData = Weather::GetWeatherData();
        std::unique_lock lock(refStateMutex);
        return refUpdater.StartUpdate(eventsToProcess, refStateMap, weatherData, delta);
    } else {
        return false;  // Still running, can't start a new update
    }
}

void WildfireMgr::PeriodicUpdate(float delta) {
    auto* set = Settings::GetSingleton();
    if (set->ModActive == false) {
        return;  // Mod is disabled, skip all processing
    }

    static float grassTimer = 0.0f;
    static float hazardTimer = 0.0f;
    static float refTimer = 0.0f;

    grassTimer += delta;
    hazardTimer += delta;
    refTimer += delta;

    if (cellStateMap.empty() && refStateMap.empty()) {
        hazardTimer = set->HazardPeriodicUpdateTime;
    } else if (hazardTimer >= set->HazardPeriodicUpdateTime) {
        hazardTimer = 0.0f;
        HazardUpdate();
        return;
    }

    if (cellStateMap.empty() && grassEventQueue.empty()) {
        grassTimer = set->GrassPeriodicUpdateTime;
    } else if (grassTimer >= set->GrassPeriodicUpdateTime) {
        if (GrassUpdate(grassTimer)) {
            grassTimer = 0.0f;
            return;
        } else {
            static int consecutiveRuns = 0;
            consecutiveRuns++;
            if (consecutiveRuns >= 5) {
                logger::error("Grass update has been running for {} consecutive updates.", consecutiveRuns);
            } else {
                logger::warn("Grass update still running");
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
            return;
        } else {
            static int consecutiveRuns = 0;
            consecutiveRuns++;
            if (consecutiveRuns >= 5) {
                logger::error("References update has been running for {} consecutive updates.", consecutiveRuns);
            } else {
                logger::warn("References update still running");
            }
        }
        // If RefUpdate returns false, we not reset the timer to try again next frame
        // This allows us to handle cases where we have more events than we can process in one update
    }

    grassUpdater.RegenerateGrass();
}

void WildfireMgr::AddGrassEvent(const RE::NiPoint3& impactPos, float radius, float damage, ElementType type) {
    static auto* set = Settings::GetSingleton();

    if (type != ElementType::Shock && type != ElementType::Neutral) {  // Shock can't affect grass
        {
            std::unique_lock lock(grassEventMutex);
            grassEventQueue.emplace_back(impactPos, radius, damage, type);
        }
        RE::TES::GetSingleton()->ForEachCellInRange(impactPos, radius, [this](RE::TESObjectCELL* cell) {
            if (cell && cell->IsExteriorCell()) {
                grassUpdater.GetOrCreateCellState(cellStateMap, cell);
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });

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
    }
}

void WildfireMgr::AddRefEvent(RE::TESObjectREFR* ref, float damage, ElementType type) {
    static auto* set = Settings::GetSingleton();

    if (ref && Utils::IsSupportedForm(ref)) {
        if (type != ElementType::Neutral) {
            auto refHandle = ref->GetHandle();
            {
                std::unique_lock lock(refEventMutex);
                refEventQueue.emplace_back(refHandle, damage, type);
            }
            refUpdater.GetOrCreateRefState(refStateMap, ref->GetHandle());
        }

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
    }
}

void WildfireMgr::SendHitEvent(RE::TESObjectREFR* ref) {
    if (ref) {
        // If is supported by OIF
        if (Utils::IsOIFSupported(ref)) {
            static auto* player = RE::PlayerCharacter::GetSingleton();
            logger::debug("Sending HitEvent for {:X} Name: {}", ref->GetFormID(), ref->GetName());
            static RE::TESHitEvent event(ref, player, 0x012FCD, 0x012FCF, RE::TESHitEvent::Flag::kNone);
            RE::ScriptEventSourceHolder::GetSingleton()->SendEvent(&event);
        }
        if (Utils::IsPapyrusSupported(ref)) {
            

            static RE::TESObjectREFR* aggressor = nullptr;
            static RE::TESForm* source = RE::TESForm::LookupByEditorID("Flames");
            static auto projectileForm = RE::TESForm::LookupByEditorID("FlamesProjectile");
            static RE::BGSProjectile* projectile = projectileForm->As<RE::BGSProjectile>();

            static bool powerAttack = false;
            static bool sneakAttack = false;
            static bool bashAttack = false;
            static bool hitBlocked = false;

            //SendPapyrusEvent(ref, "OnHit", aggressor, source, projectile, false, false, false, false);

        }
    }
}

void WildfireMgr::SetHazardsPCOnly(bool PCOnly) { hazardUpdater.SetHazardsPCOnly(PCOnly); }