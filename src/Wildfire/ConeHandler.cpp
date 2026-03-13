#include "Wildfire/Wildfire.h"

#include "Settings.h"
#include "Utils.h"
#include "Types.h"

// To be removed in release builds
#include "CLibUtilsQTR/DrawDebug.hpp"

std::unordered_map<RE::Projectile*, float> lastProcessTime;

void WildfireMgr::ProcessCone(RE::Projectile* a_proj) {
    if (!a_proj) {
        return;
    }
    auto start = std::chrono::high_resolution_clock::now();

    auto& runtime = a_proj->GetProjectileRuntimeData();
    float now = runtime.livingTime;
    constexpr float interval = 0.5f;
    auto it = lastProcessTime.find(a_proj);
    if (it != lastProcessTime.end()) {
        if (now - it->second < interval) {
            return;
        }
        it->second = now;
    } else {
        lastProcessTime[a_proj] = now;
        return;
    }

    if (lastProcessTime.size() > 32) {
        for (auto it2 = lastProcessTime.begin(); it2 != lastProcessTime.end();) {
            if (!it2->first || it2->first->IsDestroyed()) {
                it2 = lastProcessTime.erase(it2);
            } else {
                ++it2;
            }
        }
    }

    auto pos = a_proj->GetPosition();
    auto base_damage = Utils::GetDamageFromProjectile(a_proj);
    auto radius = a_proj->GetHeight() * 10.0f;
    radius = std::min(std::max(radius, float(VERTS_STEP)), float(VERTS_STEP) * 2.5f);

    static auto* set = Settings::GetSingleton();
    float adj_damage = base_damage;
    auto type = Utils::GetProjectileType(a_proj);
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
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    logger::debug("Process Cone adj_damage {} took {} ms", adj_damage, elapsed.count());
}
