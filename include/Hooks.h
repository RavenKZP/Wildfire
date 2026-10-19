#pragma once

#include "Detour.h"

namespace Hooks {

    struct DrawHook {
        static void thunk(std::uint32_t a_timer);
        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct UpdateHook {
        static void PlayerUpdate(RE::Actor* a_this, float a_delta);
        static inline REL::Relocation<decltype(PlayerUpdate)> originalPlayerUpdate;
    };

    struct MissileImpact {
        static void AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(AddImpact)> originalAddImpact;

        static void SetDelete(RE::Projectile* a_proj, bool a_set);
        static inline REL::Relocation<decltype(SetDelete)> originalSetDelete;
    };
    struct BeamImpact {
        static void AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(AddImpact)> originalAddImpact;

        static void SetDelete(RE::Projectile* a_proj, bool a_set);
        static inline REL::Relocation<decltype(SetDelete)> originalSetDelete;
    };
    struct FlameImpact {
        static void AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(AddImpact)> originalAddImpact;

        static void SetDelete(RE::Projectile* a_proj, bool a_set);
        static inline REL::Relocation<decltype(SetDelete)> originalSetDelete;
    };
    struct GrenadeImpact {
        static void AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(AddImpact)> originalAddImpact;

        static void SetDelete(RE::Projectile* a_proj, bool a_set);
        static inline REL::Relocation<decltype(SetDelete)> originalSetDelete;
    };
    struct ConeImpact {
        static void AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(AddImpact)> originalAddImpact;

        static void SetDelete(RE::Projectile* a_proj, bool a_set);
        static inline REL::Relocation<decltype(SetDelete)> originalSetDelete;
    };

    struct ConeProjectile {
        static void Update(RE::Projectile* a_proj, float a_delta);
        static inline REL::Relocation<decltype(Update)> originalUpdate;

        static void SetDelete(RE::Projectile* a_proj, bool a_set);
        static inline REL::Relocation<decltype(SetDelete)> originalSetDelete;
    };
    struct ArrowImpact {
        static void AddImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc,
                              const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6,
                              std::uint32_t a_arg7);
        static inline REL::Relocation<decltype(AddImpact)> originalAddImpact;

        static void SetDelete(RE::Projectile* a_proj, bool a_set);
        static inline REL::Relocation<decltype(SetDelete)> originalSetDelete;
    };

    struct ExplosionHook {
        static void InitializeExplosion(RE::Explosion* a_this);
        static inline REL::Relocation<decltype(InitializeExplosion)> originalInitializeExplosion;
    };


    inline void InstallHooks() {

        // for VR use PlayerCharacter::Update function, rest we can Hook BSGraphics::Renderer::End
        // PlayerCharacter::Update is not caled when in tfc mode
        if (REL::Module::IsVR()) {
            UpdateHook::originalPlayerUpdate =
                REL::Relocation<std::uintptr_t>(RE::VTABLE_PlayerCharacter[0])
                    .write_vfunc(REL::Relocate(0xAD, 0xAD, 0xAF), UpdateHook::PlayerUpdate);
        } else {
            auto& trampoline = SKSE::GetTrampoline();
            constexpr size_t size_per_hook = 14;
            trampoline.create(size_per_hook);

            const REL::Relocation<std::uintptr_t> target{
                REL::RelocationID(75461, 77246)};  // RE::BSGraphics::Renderer::End();
            DrawHook::func = trampoline.write_call<5>(target.address() + 0x9, DrawHook::thunk);
        }

        MissileImpact::originalAddImpact = REL::Relocation<std::uintptr_t>(RE::MissileProjectile::VTABLE[0])
                                               .write_vfunc(REL::Relocate(0xBD, 0xBD, 0xBE), MissileImpact::AddImpact);
        BeamImpact::originalAddImpact = REL::Relocation<std::uintptr_t>(RE::BeamProjectile::VTABLE[0])
                                            .write_vfunc(REL::Relocate(0xBD, 0xBD, 0xBE), BeamImpact::AddImpact);
        FlameImpact::originalAddImpact = REL::Relocation<std::uintptr_t>(RE::FlameProjectile::VTABLE[0])
                                             .write_vfunc(REL::Relocate(0xBD, 0xBD, 0xBE), FlameImpact::AddImpact);
        GrenadeImpact::originalAddImpact = REL::Relocation<std::uintptr_t>(RE::GrenadeProjectile::VTABLE[0])
                                               .write_vfunc(REL::Relocate(0xBD, 0xBD, 0xBE), GrenadeImpact::AddImpact);
        ArrowImpact::originalAddImpact = REL::Relocation<std::uintptr_t>(RE::ArrowProjectile::VTABLE[0])
                                             .write_vfunc(REL::Relocate(0xBD, 0xBD, 0xBE), ArrowImpact::AddImpact);

        MissileImpact::originalSetDelete = REL::Relocation<std::uintptr_t>(RE::MissileProjectile::VTABLE[0])
                                               .write_vfunc(0x23, MissileImpact::SetDelete);
        BeamImpact::originalSetDelete =
            REL::Relocation<std::uintptr_t>(RE::BeamProjectile::VTABLE[0]).write_vfunc(0x23, BeamImpact::SetDelete);
        FlameImpact::originalSetDelete =
            REL::Relocation<std::uintptr_t>(RE::FlameProjectile::VTABLE[0]).write_vfunc(0x23, FlameImpact::SetDelete);
        GrenadeImpact::originalSetDelete = REL::Relocation<std::uintptr_t>(RE::GrenadeProjectile::VTABLE[0])
                                               .write_vfunc(0x23, GrenadeImpact::SetDelete);
        ArrowImpact::originalSetDelete =
            REL::Relocation<std::uintptr_t>(RE::ArrowProjectile::VTABLE[0]).write_vfunc(0x23, ArrowImpact::SetDelete);

        ConeProjectile::originalUpdate = REL::Relocation<std::uintptr_t>(RE::ConeProjectile::VTABLE[0])
                                             .write_vfunc(REL::Relocate(0xAB, 0xAB, 0xAC), ConeProjectile::Update);
        ConeProjectile::originalSetDelete =
            REL::Relocation<std::uintptr_t>(RE::ConeProjectile::VTABLE[0]).write_vfunc(0x23, ConeProjectile::SetDelete);

        ExplosionHook::originalInitializeExplosion =
            REL::Relocation<std::uintptr_t>(RE::Explosion::VTABLE[0])
                .write_vfunc(REL::Relocate(0xA2, 0xA2, 0xA3), ExplosionHook::InitializeExplosion);


    }
}  // namespace Hooks