#pragma once

#include "Settings.h"

#include "ClibUtil/singleton.hpp"

static bool VertexCanBurn(RE::TESObjectLAND::LoadedLandData* landData, int quadrant, int vertexIdx) {
    for (int texIdx = 0; texIdx < 6; ++texIdx) {
        // Check percent coverage
        if (landData->percents[quadrant][vertexIdx][texIdx] > 0) {
            logger::trace("Texture index {} has sufficient coverage", texIdx);

            RE::TESLandTexture* tex = landData->quadTextures[quadrant][texIdx];
            if (!tex) continue;

            // Check grass list
            for (const auto& grass : tex->textureGrassList) {
                if (grass) {
                    logger::trace("Texture index {} has grass {} {}", texIdx, grass->GetName(), grass->GetFormEditorID());
                    return true;
                }
            }
            if (!tex->textureGrassList.empty()) {
                logger::trace("Texture index {} has grass list", texIdx);
                return true;
            }

            // Check material type
            logger::trace("Material type for texture index {}: {}", texIdx, tex->materialType->materialID);
            if (tex->materialType && tex->materialType->materialID == RE::MATERIAL_ID::kGrass) {
                logger::trace("Texture index {} has grass material type", texIdx);
                return true;
            }
        }
    }
    return false;
}

struct FireCellState {
    float heat[4][289];
    float fuel[4][289];
    bool isBurning[4][289];
    bool canBurn[4][289];
    bool isCharred[4][289];
    bool altered;

    FireCellState(RE::TESObjectCELL* cell) {
        auto* set = Settings::GetSingleton();
        auto cellLand = cell->GetRuntimeData().cellLand;
        for (int q = 0; q < 4; ++q) {
            for (int v = 0; v < 289; ++v) {
                heat[q][v] = 0.0f;
                fuel[q][v] = set->InitialFuelAmount;
                isBurning[q][v] = false;
                canBurn[q][v] = VertexCanBurn(cellLand->loadedData,q,v);
                isCharred[q][v] = false;
            }
        }
        altered = false;
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

    std::unordered_map<RE::TESObjectCELL*, FireCellState> fireCellMap;
};