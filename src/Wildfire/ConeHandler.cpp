#include "Wildfire/Wildfire.h"

#include "Settings.h"
#include "Utils.h"
#include "Types.h"

void WildfireMgr::ProcessCone(RE::Projectile* a_proj) {
    if (!a_proj) {
        return;
    }
    auto start = std::chrono::high_resolution_clock::now();
    auto type = Utils::GetProjectileType(a_proj);
    if (type == ElementType::Unknown || type == ElementType::Neutral) {
        return;
    }

    auto& runtimeData = a_proj->GetProjectileRuntimeData();
    float now = runtimeData.livingTime;
    float interval = 0.5f;
    auto projFormID = a_proj->GetFormID();
    
    auto projUpd = Utils::ProjectileUpdate::GetSingleton();
    auto lastProcessTime = projUpd->Get(projFormID);
    if (lastProcessTime > 0) {
        if (now - lastProcessTime < interval) {
            return;
        } else {
            projUpd->Update(projFormID, now);
        }
    } else {
        projUpd->Add(projFormID, now);
    }

    auto pos = a_proj->GetPosition();
    auto base_damage = Utils::GetDamageFromProjectile(a_proj);
    auto radius = a_proj->GetHeight() * 10.0f;
    radius = std::min(std::max(radius, float(VERTS_STEP)), float(VERTS_STEP) * 2.5f);

    static auto* set = Settings::GetSingleton();
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

    AddGrassEvent(pos, radius, adj_damage, type);

    RE::TES::GetSingleton()->ForEachReferenceInRange(pos, radius,
                                                     [this, adj_damage, type](RE::TESObjectREFR* ref) {
                                                         if (ref) {
                                                             AddRefEvent(ref, adj_damage, type);
                                                         }
                                                         return RE::BSContainer::ForEachResult::kContinue;
                                                     });
    if (set->EnableTimeLogging) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        logger::info("[Main] [Process Cone] Adj_damage {} took {} ms", adj_damage, elapsed.count());
    }
}

void WildfireMgr::RemoveCone(RE::Projectile* a_projectile) {
    auto projUpd = Utils::ProjectileUpdate::GetSingleton();
    projUpd->Remove(a_projectile->GetFormID());
}
