#include "Hooks.h"

#include "Wildfire/Wildfire.h"

#include "CLibUtilsQTR/DrawDebug.hpp"

#include "Settings.h"

namespace Hooks {
    void UpdateHook::Update(RE::Actor* a_this, float a_delta) {
        Update_(a_this, a_delta);

        static auto* set = Settings::GetSingleton();

        if (!set->ModActive) {
            return;
        }
        WildfireMgr::GetSingleton()->PeriodicUpdate(a_delta);

        if (set->DebugMode) {
            DebugAPI_IMPL::DebugAPI::GetSingleton()->Update();
        }
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
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void ConeImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                           const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                           std::uint32_t a_arg7) {
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void ArrowImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                            const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                            std::uint32_t a_arg7) {
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    // Cone - Shouts
    void ConeUpdate::thunk(RE::Projectile* a_proj, float a_delta) {
        WildfireMgr::GetSingleton()->ProcessCone(a_proj);
        originalFunction(a_proj, a_delta);
    }

    // Explosions
    void ExplosionHook::thunk(RE::Explosion* a_this) {
        originalFunction(a_this);
        WildfireMgr::GetSingleton()->ProcessExplosion(a_this);
    }


    // Expirements
    bool ParticleHook::thunk(RE::BSParticleShaderCubeEmitter* a_this, bool a_arg1, RE::NiPoint3& a_arg2,
                             RE::NiPoint3& a_arg3) {
        auto res = originalFunction(a_this, a_arg1, a_arg2, a_arg3);
        logger::debug("ParticleHook------------------------------------------------------------------------------");

        static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
        DebugAPI->DrawLineForMS(a_arg2, a_arg3, 1000, {1, 1, 1, 1}, 10);
        return res;
    }

}  // namespace Hooks