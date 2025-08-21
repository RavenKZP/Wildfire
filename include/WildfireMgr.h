#pragma once

#include "Types.h"

#include "ClibUtil/singleton.hpp"

#include <future>
#include <shared_mutex>


inline bool ContainsKeywordInsensitive(const RE::BSFixedString& modelPath, const std::string_view keyword) {
    std::string_view pathView = modelPath;
    std::string lowerPath(pathView);
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

    std::string lowerKeyword(keyword);
    std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::tolower);

    return lowerPath.find(lowerKeyword) != std::string::npos;
}

class WildfireMgr : public clib_util::singleton::ISingleton<WildfireMgr> {
public:
    void PeriodicUpdate(float delta);
    void GenerateGrassInQueueCells();

    void AddFireEvent(const RE::NiPoint3& impactPos, float radius, float damage);
    bool IsCellAltered(RE::TESObjectCELL* cell);
    void DamageFireCell(FireVertex target, float damage, bool mgr = false);
    void CoolFireCell(FireVertex target, float damage);

    std::unordered_map<RE::TESObjectCELL*, FireCellState> GetFireCellMap() const { return fireCellMap; };

    
    // Wind-related methods
    WindData GetCurrentWind();
    bool IsCurrentWeatherRaining();

    void ResetFireCellState(RE::TESObjectCELL* cell);
    void ResetAllFireCells();

private:

    // Get or create a FireCellState for the given cell
    FireCellState* GetOrCreateFireCellState(RE::TESObjectCELL* cell);

    // Vertex-related methods
    FireVertex FindNearestVertex(const RE::NiPoint3& pos);
    std::vector<FireVertex> FindNearestVertexsInRadius(const RE::NiPoint3& pos, const float radius);

    // Get neighbours of a vertex
    std::vector<FireVertex> GetFireVertexNeighbours(const FireVertex& vertex);
    std::vector<WeightedNeighbour> GetFireVertexNeighboursWeighted(const FireVertex& vertex, WindData windData);
    
    // Cell-related methods
    std::pair<int, int> GetCellCoords(RE::TESObjectCELL* cell);
    RE::TESObjectCELL* GetCellByCoords(int cellX, int cellY);

    
    std::shared_mutex fireCellMapMutex;
    std::unordered_map<RE::TESObjectCELL*, FireCellState> fireCellMap;

    std::shared_mutex cellTasksMutex;
    std::unordered_map<RE::TESObjectCELL*, std::future<void>> cellTasks;

    std::shared_mutex grassGenerationMutex;
    std::queue<RE::TESObjectCELL*> grassGenerationQueue;
};