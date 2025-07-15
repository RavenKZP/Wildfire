namespace Hooks {
    class Decal2 {
    public:
        static void Install();

    private:
        static char thunk(RE::BSTempEffectSimpleDecal* a1, RE::BSTriShape* a2);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };

    struct UpdateHook {
        static inline RE::TESObjectREFR* object = nullptr;
        static void Update(RE::Actor* a_this, float a_delta);
        static inline REL::Relocation<decltype(Update)> Update_;
        static void Install();
    };
}