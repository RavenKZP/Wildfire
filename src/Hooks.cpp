#include "Hooks.h"

#include "Wildfire/Wildfire.h"

#include "CLibUtilsQTR/DrawDebug.hpp"

#include "Settings.h"

namespace Hooks {
    void UpdateHook::Update(RE::Actor* a_this, float a_delta) {
        Update_(a_this, a_delta);

        auto* set = Settings::GetSingleton();

        if (!set->ModActive) {
            return;
        }

        if (set->DebugMode) {
            DebugAPI_IMPL::DebugAPI::GetSingleton()->Update();
        }

        WildfireMgr::GetSingleton()->PeriodicUpdate(a_delta);
    }

    // Impacts
    void MissileImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7) {
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void BeamImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                           const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                           std::uint32_t a_arg7) {
        logger::info("BeamImpact Hook");
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void FlameImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                            const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                            std::uint32_t a_arg7) {
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void GrenadeImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7) {
        logger::info("GrenadeImpact Hook");
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void ConeImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                           const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                           std::uint32_t a_arg7) {
        logger::info("ConeImpact Hook");
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void ArrowImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                            const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                            std::uint32_t a_arg7) {
        logger::info("ArrowImpact Hook");
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    // Cone - Shouts
    void ConeUpdate::thunk(RE::Projectile* a_proj, float a_delta) {
        logger::info("ConeUpdate Hook");
        WildfireMgr::GetSingleton()->ProcessCone(a_proj);
        originalFunction(a_proj, a_delta);
    }

    // Explosions
    void ExplosionHook::thunk(RE::Explosion* a_this) {
        originalFunction(a_this);
        WildfireMgr::GetSingleton()->ProcessExplosion(a_this);
    }

}  // namespace Hooks