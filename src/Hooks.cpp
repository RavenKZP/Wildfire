#include "Hooks.h"
#include "DrawDebug.h"
#include "Utils.h"
#include "Settings.h"
#include "WildfireMgr.h"
#include "HazardMgr.h"

#include <chrono>
#include <cmath>
#include <algorithm>

namespace Hooks {

    void ProcessImpact(RE::Projectile* a_proj, const RE::NiPoint3& pos) {
        if (Settings::GetSingleton()->ModActive == false) {
            return;  // If the mod is inactive, skip processing
        }

        auto TES = RE::TES::GetSingleton();
        auto cell = TES->GetCell(pos);
        if (!cell) {
            logger::warn("No cell found at position: ({}, {}, {})", pos.x, pos.y, pos.z);
            return;  // If no cell found, skip processing
        }
        if (!cell->GetRuntimeData().cellLand) {
            logger::warn("No cell land data found for cell at position: ({}, {}, {})", pos.x, pos.y, pos.z);
            return;  // If no land data, skip processing
        }

        float radius = 0.0f;
        if (a_proj->GetProjectileRuntimeData().explosion) {
            radius = a_proj->GetProjectileRuntimeData().explosion->data.radius;
            explosion_handled = true;
        }

        auto* set = Settings::GetSingleton();
        logger::info("Processing impact at position: ({}, {}, {})", pos.x, pos.y, pos.z);

        auto start = std::chrono::high_resolution_clock::now();

        auto ProjectileType = Utils::GetProjectileType(a_proj);

        if (ProjectileType == ProjectileType::Fire) {
            float damage = Utils::GetDamageFromProjectile(a_proj) * set->FireDamageMultiplayer;
            logger::info("Fire Damage {} radius: {}", damage, radius);
            WildfireMgr::GetSingleton()->AddFireEvent(pos, radius, damage);
        } else if (ProjectileType == ProjectileType::Cold) {
            float damage = Utils::GetDamageFromProjectile(a_proj) * set->ColdDamageMultiplayer;
            logger::info("Cold Damage {} radius: {}", damage, radius);
            WildfireMgr::GetSingleton()->AddFireEvent(pos, radius, -damage);
        } else if (ProjectileType == ProjectileType::Water) {
            float damage = Utils::GetDamageFromProjectile(a_proj) * set->WaterDamageMultiplayer;
            logger::info("Water Damage {} radius: {}", damage, radius);
            WildfireMgr::GetSingleton()->AddFireEvent(pos, radius, -damage);
        } else {
            logger::warn("Unknown projectile type: {}", a_proj->GetFormEditorID());
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        logger::info("Processed in {} ms", duration.count());

        if (set->DebugMode) {
            DebugAPI_IMPL::DebugAPI::DrawSphere(glm::vec3(pos.x, pos.y, pos.z), 50, 5000,
                                                glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
        }
    }

    void ProcessExplosion(RE::Explosion* exp, RE::NiPoint3 pos) {
        if (Settings::GetSingleton()->ModActive == false) {
            return;  // If the mod is inactive, skip processing
        }

        auto TES = RE::TES::GetSingleton();
        auto cell = TES->GetCell(pos);
        if (!cell) {
            logger::warn("No cell found at position: ({}, {}, {})", pos.x, pos.y, pos.z);
            return;  // If no cell found, skip processing
        }
        if (!cell->GetRuntimeData().cellLand) {
            logger::warn("No cell land data found for cell at position: ({}, {}, {})", pos.x, pos.y, pos.z);
            return;  // If no land data, skip processing
        }

        if (explosion_handled) {
            explosion_handled = false;
            return;
        }

        auto* set = Settings::GetSingleton();
        logger::info("Processing Explosion at position: ({}, {}, {})", pos.x, pos.y, pos.z);

        auto start = std::chrono::high_resolution_clock::now();

        auto ProjectileType = Utils::GetExplosionType(exp);
        auto explosionRuntimeData = exp->GetExplosionRuntimeData();

        float damage = explosionRuntimeData.damage;
        if (damage < 0.0f) {
            damage = set->DefaultExplosionDamage;
        }

        if (ProjectileType == ProjectileType::Fire) {
            logger::info("Fire Damage {} radius: {}", damage * set->FireDamageMultiplayer, explosionRuntimeData.radius);
            WildfireMgr::GetSingleton()->AddFireEvent(pos, explosionRuntimeData.radius, damage);
        } else if (ProjectileType == ProjectileType::Cold) {
            logger::info("Cold Damage {} radius: {}", damage * set->ColdDamageMultiplayer, explosionRuntimeData.radius);
            WildfireMgr::GetSingleton()->AddFireEvent(pos, explosionRuntimeData.radius, -damage);
        } else if (ProjectileType == ProjectileType::Water) {
            logger::info("Water Damage {} radius: {}", damage * set->WaterDamageMultiplayer,
                         explosionRuntimeData.radius);
            WildfireMgr::GetSingleton()->AddFireEvent(pos, explosionRuntimeData.radius, -damage);
        } else {
            logger::warn("Unknown exploson type: {}", exp->GetFormEditorID());
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        logger::info("Processed in {} ms", duration.count());

        if (set->DebugMode) {
            DebugAPI_IMPL::DebugAPI::DrawSphere(glm::vec3(pos.x, pos.y, pos.z), 50, 5000,
                                                glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
        }
    }

    void UpdateHook::Update(RE::Actor* a_this, float a_delta) {
        Update_(a_this, a_delta);

        auto* set = Settings::GetSingleton();

        if (!set->ModActive) {
            return;  // If the mod is inactive, skip the update
        }

        auto* WildfireMgr = WildfireMgr::GetSingleton();

        static float grassUpdateTimeAccumulator = 0.0f;
        grassUpdateTimeAccumulator += a_delta;
        if (grassUpdateTimeAccumulator > set->GrassPeriodicUpdateTime) {
            auto start = std::chrono::high_resolution_clock::now();
            WildfireMgr->PeriodicUpdate(grassUpdateTimeAccumulator);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            logger::debug("Grass Periodic Update in {} ms", duration.count());
            grassUpdateTimeAccumulator = 0.0f;
        }

        // Start with half the update time to make it asunchronous with grass updates
        static float hazardUpdateTimeAccumulator = set->HazardPeriodicUpdateTime / 2.0f;
        hazardUpdateTimeAccumulator += a_delta;
        if (hazardUpdateTimeAccumulator > set->HazardPeriodicUpdateTime) {
            auto start = std::chrono::high_resolution_clock::now();
            HazardMgr::GetSingleton()->PeriodicUpdate(hazardUpdateTimeAccumulator);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            logger::debug("Hazard Periodic Update in {} ms", duration.count());
            hazardUpdateTimeAccumulator = 0.0f;
        }

        WildfireMgr->GenerateGrassInQueueCells();        

        if (set->DebugMode) {
            DebugAPI_IMPL::DebugAPI::Update();
        }
    }

    void Hooks::MissileImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                     const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable,
                                     std::int32_t a_arg6, std::uint32_t a_arg7) {
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        logger::info("MissileImpact Hook");
        Hooks::ProcessImpact(a_proj, a_targetLoc);
    }

    void Hooks::BeamImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                  const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                                  std::uint32_t a_arg7) {
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        logger::info("BeamImpact Hook");
        Hooks::ProcessImpact(a_proj, a_targetLoc);
    }

    void Hooks::FlameImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                   const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                                   std::uint32_t a_arg7) {
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        logger::info("FlameImpact Hook");
        Hooks::ProcessImpact(a_proj, a_targetLoc);
    }

    void Hooks::GrenadeImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                     const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable,
                                     std::int32_t a_arg6, std::uint32_t a_arg7) {
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        logger::info("GrenadeImpact Hook");
        Hooks::ProcessImpact(a_proj, a_targetLoc);
    }

    void Hooks::ConeImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                  const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                                  std::uint32_t a_arg7) {
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        logger::info("ConeImpact Hook");
        Hooks::ProcessImpact(a_proj, a_targetLoc);
    }

    void Hooks::ArrowImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                   const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                                   std::uint32_t a_arg7) {
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        logger::info("ArrowImpact Hook");
        Hooks::ProcessImpact(a_proj, a_targetLoc);
    }

    void Hooks::ExplosionHook::thunk(RE::Explosion* a_this) {
        originalFunction(a_this);
        logger::info("Explosion Hook");
        Hooks::ProcessExplosion(a_this, a_this->GetPosition());
    }

    char DecalHook::thunk(RE::BSTempEffectSimpleDecal* a_decal, RE::BSTriShape* a2) {
        // Possibly only way to get Fire Breath
        return originalFunction(a_decal, a2);
    }
}