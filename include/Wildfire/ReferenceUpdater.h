#pragma once

#include <shared_mutex>

#include "Types.h"

class ReferenceUpdater {
public:
    ReferenceUpdater();

    void OnGameLoad();

    bool StartUpdate(RefEventType& refEvents, RefStateMapType& burningRefs, WeatherData& wheatherData, float delta);
    bool IsRunning() const;
    std::optional<RefStateMapType> GetResult();
    void ClearResult();

    // Not safe to use in thread
    RefState* GetOrCreateRefState(RefStateMapType& burningRefs, RE::ObjectRefHandle refHandle);

    // Safe to use in thread
    RefState* GetRefState(RefStateMapType& burningRefs, RE::ObjectRefHandle refHandle);

    void ApplyCharredFX(RE::TESObjectREFR* ref);
    void ApplyBurningFX(RE::TESObjectREFR* ref);
    void ApplyFrostFX(RE::TESObjectREFR* ref);
    void RemoveFX(RE::TESObjectREFR* ref);
    void RemoveLeaves(RE::TESObjectREFR* ref);
    void RemoveLeavesSmooth(RE::TESObjectREFR* ref, float progress);

private:
    RefStateMapType ReferenceUpdateLogic(RefEventType refEvents, RefStateMapType burningRefs, WeatherData wheatherData,
                                         float delta);

    RE::TESEffectShader* Wildfire_CharredFXS = nullptr;
    RE::TESEffectShader* Wildfire_BurningFXS = nullptr;
    RE::TESEffectShader* Wildfire_FrostFXS = nullptr;
    RE::TESEffectShader* Wildfire_EmptyFXS = nullptr;

    std::atomic<bool> running_;
    mutable std::mutex mutex_;
    std::optional<RefStateMapType> result_;

    std::shared_mutex createRefStateMutex;
    std::queue<RE::ObjectRefHandle> createRefStateQueue;
};