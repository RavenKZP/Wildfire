#pragma once

#include "WildfireMgr.h"

namespace Utils {

    void InitializeHazards();

    std::vector<RE::TESObjectREFR*> GetObjectsInRadius(RE::TESObjectCELL* cell, const RE::NiPoint3& origin,
                                                       float radius);
    void ChangeLandscapeColors(RE::TESObjectCELL* cell, const RE::NiPoint3& origin);
    void ChangeLandscapeColorsInRadius(const RE::NiPoint3& origin, int radiusInVertices);
    void GetCurrCellGrass();

    float GetDamageFromProjectile(RE::Projectile* proj);

    RE::NiPoint3 GetWorldPosition(const FireVertex& vertex);

}