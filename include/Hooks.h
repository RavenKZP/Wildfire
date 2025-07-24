namespace Hooks {

    void Process(RE::Projectile* a_proj, const RE::NiPoint3& a_targetLoc);

    struct UpdateHook {
        static inline RE::TESObjectREFR* object = nullptr;
        static void Update(RE::Actor* a_this, float a_delta);
        static inline REL::Relocation<decltype(Update)> Update_;
        static void Install();
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

    inline void InstallAddImpactHooks() {
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
    }
}
    