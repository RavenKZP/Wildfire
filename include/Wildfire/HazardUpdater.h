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

    void SpawnHazards();

private:
    ResultType HazardUpdateLogic(const CellStateMapType cellStateMap, const RefStateMapType burningRefs);

    void SpawnHazard(const RE::NiPoint3& position, HazardType size, bool jitter = true);

    RE::BGSHazard* Wildfire_LargeHazard;   // Vanilla large hazard x4 size
    RE::BGSHazard* Wildfire_BigHazard;     // Vanilla large hazard x2 size
    RE::BGSHazard* Wildfire_MediumHazard;  // Same as vanilla large hazard
    RE::BGSHazard* Wildfire_SmallHazard;   // Same as vanilla small hazard

    std::atomic<bool> running_;
    mutable std::mutex mutex_;
    std::optional<ResultType> result_;
    ResultType lastResult_;
};