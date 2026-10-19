#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

#include "Types.h"

class HazardUpdater {
public:
    using ResultType = std::vector<SpawnHazardData>;

    HazardUpdater();

    bool StartUpdate(const CellStateMapType& cellStateMap, const RefStateMapType& burningRefs);

    void OnGameLoad();

    bool IsRunning() const;
    std::optional<ResultType> GetResult();
    void CopyResult();
    void ClearResult();

    void ManageHazards();

    void SpawnSmokeHazardAt(const RE::NiPoint3& position);

    void SetHazardsLifetine(float lifetime);
    void SetHazardsPCOnly(bool PCOnly);

private:
    ResultType HazardUpdateLogic(const CellStateMapType cellStateMap, const RefStateMapType burningRefs);

    void SpawnHazard(const RE::NiPoint3& position, HazardType size, bool jitter = true);
    void MoveHazard(const RE::NiPoint3& pos, RE::Hazard* hazardRef, HazardType size, bool jitter = true);
    

    RE::BGSHazard* Wildfire_LargeHazard;   // Vanilla large hazard x4 size
    RE::BGSHazard* Wildfire_BigHazard;     // Vanilla large hazard x2 size
    RE::BGSHazard* Wildfire_MediumHazard;  // Same as vanilla large hazard
    RE::BGSHazard* Wildfire_SmallHazard;   // Same as vanilla small hazard

    std::vector<RE::BGSHazard*> Wildfire_SmokeHazards;   // Vanilla models as hazards so game handles their lifetime

    std::mutex FireHazardsHandlesMutex;
    std::vector<RE::ObjectRefHandle> ActiveLargeHazardsHandles;
    std::vector<RE::ObjectRefHandle> ActiveBigHazardsHandles;
    std::vector<RE::ObjectRefHandle> ActiveMediumHazardsHandles;
    std::vector<RE::ObjectRefHandle> ActiveSmallHazardsHandles;

    std::mutex SmokeHazardsHandlesMutex;
    std::vector<RE::ObjectRefHandle> ActiveSmokeHazardsHandles;

    std::atomic<bool> running_;
    mutable std::mutex mutex_;
    std::optional<ResultType> result_;
    ResultType lastResult_;
};