#include "Wildfire/Wildfire.h"

#include "Utils.h"
#include "Settings.h"

void WildfireMgr::ProcessExplosion(RE::Explosion* a_explosion) {
    auto start = std::chrono::high_resolution_clock::now();
    static auto* set = Settings::GetSingleton();
    if (!a_explosion) {
        return;
    }
    auto type = Utils::GetExplosionType(a_explosion);
    if (type == ElementType::Unknown || type == ElementType::Neutral) {
        return;
    }

    auto pos = a_explosion->GetPosition();
    auto& explosionRuntimeData = a_explosion->GetExplosionRuntimeData();
    float radius = explosionRuntimeData.radius;
    if (radius < VERTS_STEP / 2) {
        radius = VERTS_STEP / 2;  // Minimum radius to ensure we affect at least the nearest vertex
    }
    float base_damage = explosionRuntimeData.damage;

    if (base_damage <= 0.0f) {
        base_damage = radius / 10;
    }

    float adj_damage = base_damage;
    switch (type) {
        case ElementType::Fire:
            adj_damage = base_damage * set->FireDamageMultiplayer;
            break;
        case ElementType::Shock:
            adj_damage = base_damage * set->ShockDamageMultiplayer;
            break;
        case ElementType::Cold:
            adj_damage = base_damage * set->ColdDamageMultiplayer * (-1);
            break;
        case ElementType::Water:
            adj_damage = base_damage * set->WaterDamageMultiplayer * (-1);
            break;
    }

    if (type != ElementType::Shock) {
        AddGrassEvent(pos, radius, adj_damage, type);
    }

    RE::TES::GetSingleton()->ForEachReferenceInRange(a_explosion, radius,
                                                     [this, adj_damage, pos, radius, type](RE::TESObjectREFR* ref) {
                                                         if (ref) {
                                                             auto refPos = ref->GetPosition();
                                                             auto distance = pos.GetDistance(refPos);
                                                             float adjusted_damage = adj_damage * (distance / radius);
                                                             AddRefEvent(ref, adjusted_damage, type);
                                                         }
                                                         return RE::BSContainer::ForEachResult::kContinue;
                                                     });
    if (set->EnableTimeLogging) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        logger::info("Process Explosion: base_damage {}, radius {} took {} ms", adj_damage, radius, elapsed.count());
    }
}