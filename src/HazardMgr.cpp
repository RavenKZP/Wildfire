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

static float RandomFloat(float min, float max) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

void HazardMgr::SpawnFxAtVertex(const FireVertex& vertex, float lifetime, const char* fxModel) {
    if (!vertex.cell) return;
    RE::NiPoint3 pos = Utils::GetWorldPosition(vertex);
    pos.x += RandomFloat(-10.0f, 10.0f);  // Add some random offset to the position
    pos.y += RandomFloat(-10.0f, 10.0f);

    RE::NiPoint3 rotation{RandomFloat(0.0f, 360.0f), RandomFloat(0.0f, 360.0f), RandomFloat(0.0f, 360.0f)};
    std::uint32_t flags = 0;
    float scale = RandomFloat(0.8f, 1.2f);
    RE::NiAVObject* target = nullptr;

    // Spawn the particle effect Find out why only one max particle is spawned
    auto particle =
        RE::BSTempEffectParticle::Spawn(vertex.cell, lifetime, fxModel, rotation, pos, scale, flags, target);
    if (!particle) {
        logger::error("Failed to spawn particle effect at vertex ({}, {}, {}) in cell {:X}", pos.x, pos.y, pos.z,
                      vertex.cell->GetFormID());
        return;
    }
    particle->Detach();
}

void HazardMgr::SpawnHazardAtVertex(const FireVertex& vertex, RE::BGSHazard* hazardForm,
                                    float lifetime) {
    if (!vertex.cell || !hazardForm) return;

    RE::NiPoint3 pos = Utils::GetWorldPosition(vertex);
    pos.x += RandomFloat(-50.0f, 50.0f);  // Add some random offset to the position
    pos.y += RandomFloat(-50.0f, 50.0f);

    auto origlifetime = hazardForm->data.lifetime;
    hazardForm->data.lifetime = lifetime;

    auto player = RE::PlayerCharacter::GetSingleton();
    auto hazardRef = player->PlaceObjectAtMe(hazardForm, false);
    if (!hazardRef) {
        logger::error("Failed to place hazard at vertex ({}, {}, {}) in cell {:X}", pos.x, pos.y, pos.z,
                      vertex.cell->GetFormID());
        return;
    }

    hazardRef->SetPosition(pos);
    hazardRef->data.angle = RE::NiPoint3{RandomFloat(0.0f, 360.0f), 0.0f, 0.0f};

    float scale = RandomFloat(0.8f, 1.2f);
    hazardForm->data.radius = scale;
    hazardRef->GetReferenceRuntimeData().refScale = static_cast<std::uint16_t>(scale * 100.0f);

    hazardForm->data.lifetime = origlifetime;  // Restore original lifetime
}