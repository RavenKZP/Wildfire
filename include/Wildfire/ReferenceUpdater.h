#pragma once

#include <mutex>

#include "Types.h"

class ReferenceUpdater {
public:
    ReferenceUpdater();

    void OnGameLoad();
    void HazardLoaded(RE::TESObjectREFR* ref);
    void RefLoaded(RE::TESObjectREFR* ref, RefState state);

    bool StartUpdate(RefEventType& refEvents, RefStateMapType& burningRefs, WeatherData& wheatherData, float delta);
    bool IsRunning() const;
    std::optional<RefStateMapType> GetResult();
    void ClearResult();

    void TransformObjects(float delta);

    // Not safe to use in thread
    RefState* GetOrCreateRefState(RefStateMapType& burningRefs, RE::ObjectRefHandle refHandle);

    // Safe to use in thread
    RefState* GetRefState(RefStateMapType& burningRefs, RE::ObjectRefHandle refHandle);

    void ResetAllObjects(RefStateMapType& burningRefs);

    // FX
    void ApplyCharredFX(RE::TESObjectREFR* ref);
    void ApplyCharredInstantFXS(RE::TESObjectREFR* ref);
    void ApplyBurningFX(RE::TESObjectREFR* ref);
    void ApplyFrostFX(RE::TESObjectREFR* ref);
    void ApplyDesintegrateFX(RE::ObjectRefHandle refHandle, RE::TESBoundObject* baseObjectSwap = nullptr);
    void RemoveFX(RE::TESObjectREFR* ref);

    // Model Manipulation
    void RemoveLeaves(RE::TESObjectREFR* ref);
    void RemoveStatic(RE::TESObjectREFR* ref);
    void RemoveLeavesSmooth(RE::TESObjectREFR* ref);
    void RemoveCollision(RE::TESObjectREFR* ref);
    void MeltObject(RE::TESObjectREFR* ref, RE::TESBoundObject* baseObjectSwap, float meltingProgress);
    void FadeOutObject(RE::TESObjectREFR* ref, RE::TESBoundObject* baseObjectSwap, float fadeOutProgress);

    // Effects
    void SpawnSmokeAndDecal(RE::TESObjectREFR* ref);

private:
    RefStateMapType ReferenceUpdateLogic(RefEventType refEvents, RefStateMapType burningRefs, WeatherData wheatherData,
                                         float delta);

    RE::TESEffectShader* CharredFXS = nullptr;
    RE::TESEffectShader* CharredInstantFXS = nullptr;
    RE::TESEffectShader* BurningFXS = nullptr;
    RE::TESEffectShader* FrostFXS = nullptr;
    RE::TESEffectShader* DesintegrateFXS = nullptr;
    RE::TESEffectShader* EmptyFXS = nullptr;

    std::vector<RE::TESBoundObject*> DefaultBurnedBranches;

    std::vector<TransformingData> MeltingObjects;
    std::vector<TransformingData> FadingObjects;

    int DummyHazardIterator = 0;
    std::vector<RE::BGSHazard*> DummyHazards;

    std::atomic<bool> running_;
    mutable std::mutex mutex_;
    std::optional<RefStateMapType> result_;

    mutable std::mutex createRefStateMutex;
    std::queue<RE::ObjectRefHandle> createRefStateQueue;
};