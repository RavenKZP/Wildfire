#pragma once

#include "Settings.h"

#include "ClibUtil/singleton.hpp"

struct FireCellState {
    float heat[4][289];
    float fuel[4][289];
    bool isBurning[4][289];
    RE::TESObjectREFR* hazard[4][289];
    bool canBurn[4][289];
    bool isCharred[4][289];
    bool altered = false;

    FireCellState() {
        auto* set = Settings::GetSingleton();
        for (int q = 0; q < 4; ++q) {
            for (int v = 0; v < 289; ++v) {
                heat[q][v] = 0.0f;
                fuel[q][v] = set->InitialFuelAmount;
                isBurning[q][v] = false;
                hazard[q][v] = nullptr;
                canBurn[q][v] = true;
                isCharred[q][v] = false;
            }
        }
    }
};

struct FireVertex {
    RE::TESObjectCELL* cell;  // Pointer to the cell containing this vertex
    int quadrant;  // 0-3
    int vertex;    // 0-288
};

class WildfireMgr : public clib_util::singleton::ISingleton<WildfireMgr> {
public:
    void PeriodicUpdate(float delta);

    void AddFireEvent(const RE::NiPoint3& impactPos, float radius, float damage);
    bool IsCellAltered(RE::TESObjectCELL* cell);
    void DamageFireCell(FireVertex target, float damage);
    void CoolFireCell(FireVertex target, float damage);

    std::unordered_map<RE::TESObjectCELL*, FireCellState> GetFireCellMap() const { return fireCellMap; };

private:

    FireVertex FindNearestVertex(const RE::NiPoint3& pos);
    FireCellState* GetOrCreateFireCellState(RE::TESObjectCELL* cell);
    std::vector<FireVertex> GetFireVertexNeighbours(const FireVertex& vertex);
    std::vector<FireVertex> GetFireVertexNeighboursInRadus(const FireVertex& vertex, const float radius);

    std::pair<int, int> GetCellCoords(RE::TESObjectCELL* cell);
    RE::TESObjectCELL* GetCellByCoords(int cellX, int cellY);
    RE::NiPoint3 GetWorldPosition(const FireVertex& vertex);

    void SpawnFxAtVertex(const FireVertex& vertex, float lifetime, const char* fxModel);
    void SpawnHazardAtVertex(const FireVertex& vertex, RE::BGSHazard* hazardForm);
    void DeleteHazardAtVertex(const FireVertex& vertex);

    std::unordered_map<RE::TESObjectCELL*, FireCellState> fireCellMap;
};