namespace Hooks {

    void ProcessImpact(RE::Projectile* a_proj, const RE::NiPoint3& a_targetLoc);


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

    struct DecalHook {
        static char thunk(RE::BSTempEffectSimpleDecal* a_decal, RE::BSTriShape* a2);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };

    inline void InstallHooks() {
        constexpr size_t size_per_hook = 14;
        auto& trampoline = SKSE::GetTrampoline();
        SKSE::AllocTrampoline(size_per_hook * 1);

        DecalHook::originalFunction = trampoline.write_call<5>(
            REL::RelocationID(29250, 30104).address() + REL::Relocate(0x10, 0x10), DecalHook::thunk);

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
        ConeImpact::originalFunction =
            REL::Relocation<std::uintptr_t>(RE::ConeProjectile::VTABLE[0]).write_vfunc(0xBD, ConeImpact::thunk);
        ArrowImpact::originalFunction =
            REL::Relocation<std::uintptr_t>(RE::ArrowProjectile::VTABLE[0]).write_vfunc(0xBD, ArrowImpact::thunk);
        ExplosionHook::originalFunction = 
            REL::Relocation<std::uintptr_t>(RE::Explosion::VTABLE[0]).write_vfunc(0xA2, ExplosionHook::thunk);


    }

    inline bool explosion_handled = false;
}
    