#pragma once

namespace Hooks {

    struct UpdateHook {
        static void Update(RE::Actor* a_this, float a_delta);
        static inline REL::Relocation<decltype(Update)> Update_;
    };

    struct MissileImpact {
        static void thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                          const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                          std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };
    struct BeamImpact {
        static void thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                          const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                          std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };
    struct FlameImpact {
        static void thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                          const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                          std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };
    struct GrenadeImpact {
        static void thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                          const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                          std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };
    struct ConeImpact {
        static void thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                          const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                          std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };
    struct ConeUpdate {
        static void thunk(RE::Projectile* a_proj, float a_delta);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };
    struct ArrowImpact {
        static void thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                          const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                          std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };

    struct ExplosionHook {
        static void thunk(RE::Explosion* a_this);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };

    inline void InstallHooks() {

        UpdateHook::Update_ =
            REL::Relocation<std::uintptr_t>(RE::VTABLE_PlayerCharacter[0]).write_vfunc(0xAD, UpdateHook::Update);

        MissileImpact::originalFunction =
            REL::Relocation<std::uintptr_t>(RE::MissileProjectile::VTABLE[0]).write_vfunc(0xBD, MissileImpact::thunk);
        BeamImpact::originalFunction =
            REL::Relocation<std::uintptr_t>(RE::BeamProjectile::VTABLE[0]).write_vfunc(0xBD, BeamImpact::thunk);
        FlameImpact::originalFunction =
            REL::Relocation<std::uintptr_t>(RE::FlameProjectile::VTABLE[0]).write_vfunc(0xBD, FlameImpact::thunk);
        GrenadeImpact::originalFunction =
            REL::Relocation<std::uintptr_t>(RE::GrenadeProjectile::VTABLE[0]).write_vfunc(0xBD, GrenadeImpact::thunk);
        ArrowImpact::originalFunction =
            REL::Relocation<std::uintptr_t>(RE::ArrowProjectile::VTABLE[0]).write_vfunc(0xBD, ArrowImpact::thunk);

        // Only Actors get ConeImpact
        // We probably need to use "collisions", but its not RE to usable state yet
        // ConeImpact::originalFunction =
        //    REL::Relocation<std::uintptr_t>(RE::ConeProjectile::VTABLE[0]).write_vfunc(0xBD, ConeImpact::thunk);
        ConeUpdate::originalFunction =
            REL::Relocation<std::uintptr_t>(RE::ConeProjectile::VTABLE[0]).write_vfunc(0xAB, ConeUpdate::thunk);

        ExplosionHook::originalFunction =
            REL::Relocation<std::uintptr_t>(RE::Explosion::VTABLE[0]).write_vfunc(0xA2, ExplosionHook::thunk);
    }
}  // namespace Hooks