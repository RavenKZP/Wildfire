#include "Hooks.h"
#include "DrawDebug.h"
#include "Utils.h"
#include "Settings.h"
#include "WildfireMgr.h"

#include <chrono>
#include <cmath>
#include <algorithm>

namespace Hooks {


    void Process(RE::Projectile* a_proj, const RE::NiPoint3& pos) {

        logger::info("Processing impact at position: ({}, {}, {})", pos.x, pos.y, pos.z);

        auto start = std::chrono::high_resolution_clock::now();
        // Check for Fire damage, use keywords, Projectile type, name, etc.

        float radius = 0.0f;
        RE::BGSExplosion* explosion = a_proj->GetProjectileRuntimeData().explosion;
        if (explosion) {
            radius = explosion->data.radius;
        }

        logger::info("Explosion strenght radius: {}", radius);

        float damage = Utils::GetDamageFromProjectile(a_proj) / 2.0f;
        WildfireMgr::GetSingleton()->AddFireEvent(pos, radius, damage);


        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        logger::info("Processed in {} ms", duration.count());

        DebugAPI_IMPL::DebugAPI::DrawSphere(glm::vec3(pos.x, pos.y, pos.z), 50, 5000,
                                            glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
    }

    void UpdateHook::Update(RE::Actor* a_this, float a_delta) {
        Update_(a_this, a_delta);

        auto* set = Settings::GetSingleton();

        static float timeAccumulator = 0.0f;
        timeAccumulator += a_delta;
        if (timeAccumulator > set->PeriodicUpdateTime) {
            auto start = std::chrono::high_resolution_clock::now();
            WildfireMgr::GetSingleton()->PeriodicUpdate(timeAccumulator);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            logger::info("Periodic Update in {} ms", duration.count());
            timeAccumulator = 0.0f;
        }
        DebugAPI_IMPL::DebugAPI::Update();
    }

    void UpdateHook::Install() {

        REL::Relocation<std::uintptr_t> PlayerCharacterVtbl{RE::VTABLE_PlayerCharacter[0]};
        Update_ = PlayerCharacterVtbl.write_vfunc(0xAD, Update);

    }

    void Hooks::MissileImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                     const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable,
                                     std::int32_t a_arg6, std::uint32_t a_arg7) {
        logger::info("MissileImpact Hook");
        Hooks::Process(a_proj, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void Hooks::BeamImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                  const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                                  std::uint32_t a_arg7) {
        logger::info("BeamImpact Hook");
        Hooks::Process(a_proj, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void Hooks::FlameImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                   const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                                   std::uint32_t a_arg7) {
        logger::info("FlameImpact Hook");
        Hooks::Process(a_proj, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void Hooks::GrenadeImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                     const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable,
                                     std::int32_t a_arg6, std::uint32_t a_arg7) {
        logger::info("GrenadeImpact Hook");
        Hooks::Process(a_proj, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void Hooks::ConeImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                  const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                                  std::uint32_t a_arg7) {
        logger::info("ConeImpact Hook");
        Hooks::Process(a_proj, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    void Hooks::ArrowImpact::thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                                  const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                                  std::uint32_t a_arg7) {
        logger::info("ArrowImpact Hook");
        Hooks::Process(a_proj, a_targetLoc);
        originalFunction(a_proj, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }
}