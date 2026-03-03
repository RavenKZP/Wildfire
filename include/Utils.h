#pragma once

#include "Types.h"

namespace Utils {
    std::pair<int, int> GetCellCoords(RE::TESObjectCELL* cell);

    RE::TESObjectCELL* GetCellByCoords(int cellX, int cellY);
    RE::TESObjectCELL* GetCellByCoords(const CellAsCoords& coords);

    RE::NiPoint3 GetPosition(const RE::TESObjectREFR* obj);
    BoundaryBox GetBoundingBox(const RE::TESObjectREFR* a_obj);

    bool IsSupportedForm(RE::TESObjectREFR* ref);

    std::string ToLower(std::string s);

    ImpactType GetProjectileType(RE::Projectile* proj);
    ImpactType GetExplosionType(RE::Explosion* exp);
}