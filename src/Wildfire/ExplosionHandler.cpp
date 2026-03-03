#include "Wildfire/Wildfire.h"

#include "Utils.h"
#include "Settings.h"

void WildfireMgr::ProcessExplosion(RE::Explosion* exp) {
    if (!exp) {
        return;
    }
    auto pos = exp->GetPosition();
    auto& explosionRuntimeData = exp->GetExplosionRuntimeData();
    float radius = explosionRuntimeData.radius;
    if (radius <= VERTS_STEP / 2) {
        radius = VERTS_STEP / 2;  // Minimum radius to ensure we affect at least the nearest vertex
    }
    float base_damage = explosionRuntimeData.damage;

    if (base_damage <= 0.0f) {
        base_damage = radius / 10;
    }

    static auto* set = Settings::GetSingleton();
    float adj_damage = base_damage;
    auto type = Utils::GetExplosionType(exp);
    switch (type) {
        case ImpactType::Fire:
            adj_damage = base_damage * set->FireDamageMultiplayer;
            break;
        case ImpactType::Shock:
            adj_damage = base_damage * set->ShockDamageMultiplayer;
            break;
        case ImpactType::Cold:
            adj_damage = base_damage * set->ColdDamageMultiplayer * (-1);
            break;
        case ImpactType::Water:
            adj_damage = base_damage * set->WaterDamageMultiplayer * (-1);
            break;
    }
    logger::debug("explosion damage: {}", adj_damage);
    AddGrassEvent(pos, radius, adj_damage, type);

    RE::TES::GetSingleton()->ForEachReferenceInRange(exp, radius,
                                                     [this, adj_damage, pos, radius, type](RE::TESObjectREFR* ref) {
                                                         if (ref) {
                                                             auto refPos = ref->GetPosition();
                                                             auto distance = pos.GetDistance(refPos);
                                                             float adjusted_damage = adj_damage * (distance / radius);
                                                             AddRefEvent(ref, adjusted_damage, type);
                                                         }
                                                         return RE::BSContainer::ForEachResult::kContinue;
                                                     });
}