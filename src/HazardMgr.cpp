#include "HazardMgr.h"
#include "WildfireMgr.h"
#include "Settings.h"
#include "Utils.h"

#include <random>

void HazardMgr::InitializeHazards() {
    FireLgShortHazard = RE::TESForm::LookupByEditorID("FireLgShortHazard")->As<RE::BGSHazard>();
    FireSmLongHazard = RE::TESForm::LookupByEditorID("FireSmLongHazard")->As<RE::BGSHazard>();
    FireSmShortHazard = RE::TESForm::LookupByEditorID("FireSmShortHazard")->As<RE::BGSHazard>();
    TrapOilHazard01 = RE::TESForm::LookupByEditorID("TrapOilHazard01")->As<RE::BGSHazard>();
    FireDragonHazard = RE::TESForm::LookupByEditorID("FireDragonHazard")->As<RE::BGSHazard>();
}

void HazardMgr::PeriodicUpdate(float delta) { 
    auto set = Settings::GetSingleton();

    std::unique_lock lock(burnGridMutex);

    /*
    auto clusters = FindBurningClusters(burnGrid);

    for (const auto& cluster : clusters) {
        if (cluster.size == 3x3)
            SpawnUltraHazard(cluster);
        else if (cluster.size == 2x2)
            SpawnMediumHazard(cluster);
        else if (cluster.size == 2x1 || 1x2)
            SpawnStripHazard(cluster);
        else
            SpawnStandardHazard(cluster);
    }
    */

    for (auto it = burnGrid.begin(); it != burnGrid.end();) {
        it->second -= delta;
        if (it->second <= 0.0f) {
            it = burnGrid.erase(it);
        } else {
            RE::NiPoint3 pos;
            pos.x = it->first.x;
            pos.y = it->first.y;
            pos.z = 0.0f;  // Z is calculated based on the terrain height
            SpawnHazardAt(pos, FireDragonHazard, set->HazardPeriodicUpdateTime);
            ++it;
        }
    }
}


void HazardMgr::CreateBurningVertex(const FireVertex& vertex, float lifetime) {
    std::unique_lock lock(burnGridMutex);
    auto coordinates = Utils::GetWorldPosition(vertex);
    HazardGridCoord tempHazGirdCell{coordinates.x, coordinates.y};
    burnGrid.emplace(tempHazGirdCell, lifetime);
}

static float RandomFloat(float min, float max) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

void HazardMgr::SpawnHazardAt(RE::NiPoint3 pos, RE::BGSHazard* hazardForm,
                                    float lifetime) {
    if (!hazardForm) return;

    pos.x += RandomFloat(-50.0f, 50.0f);  // Add some random offset to the position
    pos.y += RandomFloat(-50.0f, 50.0f);
    float z = 0.0f;
    RE::TES::GetSingleton()->GetLandHeight(pos, z);  // Get the terrain height at the position
    pos.z = z;

    auto origlifetime = hazardForm->data.lifetime;
    hazardForm->data.lifetime = lifetime;

    auto player = RE::PlayerCharacter::GetSingleton();
    auto hazardRef = player->PlaceObjectAtMe(hazardForm, false);
    if (!hazardRef) {
        logger::error("Failed to place hazard at vertex ({}, {}, {})", pos.x, pos.y, pos.z);
        return;
    }

    hazardRef->SetPosition(pos);
    hazardRef->data.angle = RE::NiPoint3{RandomFloat(0.0f, 360.0f), 0.0f, 0.0f};

    float scale = RandomFloat(0.8f, 1.2f);
    hazardForm->data.radius = scale;
    hazardRef->GetReferenceRuntimeData().refScale = static_cast<std::uint16_t>(scale * 100.0f);

    hazardForm->data.lifetime = origlifetime;  // Restore original lifetime
}