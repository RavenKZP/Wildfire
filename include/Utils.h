#pragma once

#include "Types.h"
#include "Settings.h"

namespace Utils {

    float GetDamageFromProjectile(RE::Projectile* proj);
    RE::NiPoint3 GetWorldPosition(const FireVertex& vertex);

    std::string ToLower(std::string s);

    std::tuple<bool, uint8_t, uint8_t> GetGrassData(RE::TESObjectLAND::LoadedLandData* loadedData, int q, int v);

    ProjectileType GetProjectileType(RE::Projectile* proj);
    ProjectileType GetExplosionType(RE::Explosion* exp);



}