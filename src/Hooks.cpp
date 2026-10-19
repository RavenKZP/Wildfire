#include "Hooks.h"
#include "Settings.h"

#include "Wildfire/Wildfire.h"

#include "SKSEMCP/SKSEMenuFramework.hpp"

#include "CLibUtilsQTR/DrawDebug.hpp"


namespace Hooks {
    void UpdateHook::PlayerUpdate(RE::Actor* a_this, float a_delta) {
        originalPlayerUpdate(a_this, a_delta);

        SKSE::GetTaskInterface()->AddTask([a_delta]() {
            static auto* set = Settings::GetSingleton();
            if (!set->ModActive) {
                return;
            }
            auto start = std::chrono::high_resolution_clock::now();

            if (!SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
                WildfireMgr::GetSingleton()->PeriodicUpdate(a_delta);
                if (set->EnableTimeLogging) {
                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double, std::milli> elapsed = end - start;
                    logger::info("WildfireMgr Update {}ms", elapsed.count());
                }
            }

            static float refresh = 0.0f;
            if (set->DebugMode || refresh > 0.0f) {
                DebugAPI_IMPL::DebugAPI::GetSingleton()->Update();
                if (refresh <= 0.0f) {
                    refresh = set->DebugDrawDuration;
                } else {
                    refresh -= a_delta;
                }
            }
        });
    }

    void DrawHook::thunk(std::uint32_t a_timer) {
        func(a_timer);
        SKSE::GetTaskInterface()->AddTask([]() {
            static auto* set = Settings::GetSingleton();
            if (!set->ModActive) {
                return;
            }
            static auto lastFrameTime = std::chrono::high_resolution_clock::now();
            auto FrameTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float, std::milli> deltaTime = FrameTime - lastFrameTime;
            auto a_delta = deltaTime.count() / 1000.0f;

            if (!RE::UI::GetSingleton()->GameIsPaused() && !SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
                WildfireMgr::GetSingleton()->PeriodicUpdate(a_delta);
                if (set->EnableTimeLogging) {
                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double, std::milli> elapsed = end - FrameTime;
                    logger::info("WildfireMgr Update {}ms", elapsed.count());
                }
            }
            static float refresh = 0.0f;
            if (set->DebugMode || refresh > 0.0f) {
                DebugAPI_IMPL::DebugAPI::GetSingleton()->Update();
                if (refresh <= 0.0f) {
                    refresh = set->DebugDrawDuration;
                } else {
                    refresh -= a_delta;
                }
            }
            lastFrameTime = FrameTime;
        });
    }

    // Impacts
    void MissileImpact::AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7) {
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalAddImpact(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void BeamImpact::AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                           const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                           std::uint32_t a_arg7) {
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalAddImpact(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void FlameImpact::AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                            const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                            std::uint32_t a_arg7) {
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalAddImpact(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void GrenadeImpact::AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7) {
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalAddImpact(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void ArrowImpact::AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                            const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                            std::uint32_t a_arg7) {
        WildfireMgr::GetSingleton()->ProcessImpact(a_proj, a_ref, a_targetLoc);
        originalAddImpact(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void MissileImpact::SetDelete(RE::Projectile* a_proj, bool a_set) {
        if (a_set) {
            WildfireMgr::GetSingleton()->RemoveProjectile(a_proj);
        }
        originalSetDelete(a_proj, a_set);
    }
    void BeamImpact::SetDelete(RE::Projectile* a_proj, bool a_set) {
        if (a_set) {
            WildfireMgr::GetSingleton()->RemoveProjectile(a_proj);
        }
        originalSetDelete(a_proj, a_set);
    }
    void FlameImpact::SetDelete(RE::Projectile* a_proj, bool a_set) {
        if (a_set) {
            WildfireMgr::GetSingleton()->RemoveProjectile(a_proj);
        }
        originalSetDelete(a_proj, a_set);
    }
    void GrenadeImpact::SetDelete(RE::Projectile* a_proj, bool a_set) {
        if (a_set) {
            WildfireMgr::GetSingleton()->RemoveProjectile(a_proj);
        }
        originalSetDelete(a_proj, a_set);
    }
    void ArrowImpact::SetDelete(RE::Projectile* a_proj, bool a_set) {
        if (a_set) {
            WildfireMgr::GetSingleton()->RemoveProjectile(a_proj);
        }
        originalSetDelete(a_proj, a_set);
    }
    

    // Cone - Shouts and some spells
    void ConeProjectile::Update(RE::Projectile* a_proj, float a_delta) {
        WildfireMgr::GetSingleton()->ProcessCone(a_proj);
        originalUpdate(a_proj, a_delta);
    }

    void ConeProjectile::SetDelete(RE::Projectile* a_proj, bool a_set) {
        if (a_set) {
            WildfireMgr::GetSingleton()->RemoveCone(a_proj);
        }
        originalSetDelete(a_proj, a_set);
    }

    // Explosions
    void ExplosionHook::InitializeExplosion(RE::Explosion* a_this) {
        originalInitializeExplosion(a_this);
        WildfireMgr::GetSingleton()->ProcessExplosion(a_this);
    }

}  // namespace Hooks