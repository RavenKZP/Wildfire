#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <tuple>
#include <vector>

#include "Types.h"

class GrassUpdater {
public:
    GrassUpdater();

    bool StartUpdate(GrassEventType& events, CellStateMapType& cellStateMap, WeatherData& wheatherData, float dellta);

    void OnGameLoad();

    bool IsRunning() const;
    std::optional<CellStateMapType> GetResult();
    void ClearResult();

    void RegenerateGrass();
    void RegenerateGrassInCell(RE::TESObjectCELL* cell);
    void DarkenGrassShader(RE::TESObjectCELL* cell);

    void LazyCreateCellState(CellStateMapType& cellStateMap);

    // Not safe to use in thread
    CellState* GetOrCreateCellState(CellStateMapType& cellStateMap, RE::TESObjectCELL* cell);

    // Safe to use in thread
    CellState* GetCellState(CellStateMapType& cellStateMap, RE::TESObjectCELL* cell);

    FireVertex FindNearestVertex(const RE::NiPoint3& pos);
    std::vector<FireVertex> FindNearestVertexsInRadius(const RE::NiPoint3& pos, float radius);
    std::vector<FireVertex> GetFireVertexNeighbours(const FireVertex& vertex);
    std::vector<WeightedNeighbour> GetFireVertexNeighboursWeighted(const FireVertex& vertex, WindData windData);

private:
    CellStateMapType GrassUpdateLogic(GrassEventType events, CellStateMapType cellStateMap, WeatherData wheatherData,
                                      float delta);

    std::atomic<bool> running_;
    mutable std::mutex mutex_;
    std::optional<CellStateMapType> result_;

    std::shared_mutex createCellStateMutex;
    std::queue<RE::TESObjectCELL*> createCellStateQueue;

    std::shared_mutex cellsToUpdateMutex;
    std::map<RE::TESObjectCELL*, CellState> cellsToUpdate;
};