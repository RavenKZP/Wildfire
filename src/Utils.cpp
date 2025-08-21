#include "Utils.h"	
#include "DrawDebug.h"
#include "WildfireMgr.h"
#include "Types.h"

#include <unordered_set>

namespace Utils {

    float GetDamageFromProjectile(RE::Projectile* proj) {
        if (!proj) {
            return 0.0f;
        }

        auto* set = Settings::GetSingleton();

        float ret = set->DefaultDamage;

        const auto& data = proj->GetProjectileRuntimeData();

        RE::Actor* actor = nullptr;
        if (data.shooter) {
            actor = data.shooter.get().get()->As<RE::Actor>();
        }

        if (data.weaponSource) {
            if (data.weaponSource->attackDamage > 0) {
                ret = static_cast<float>(data.weaponSource->attackDamage);
            }
        }

        if (data.ammoSource) {
            if (data.ammoSource->data.damage > 0) {
                ret = static_cast<float>(data.ammoSource->data.damage);
            }
        }

        if (data.spell) {
            float damage = 0.0f;
            if (actor) {
                damage = data.spell->CalculateMagickaCost(actor);
            } else {
                damage = data.spell->CalculateMagickaCost(nullptr);
            }
            if (damage > 0.0f) {
                ret = damage;
            }
        }
        if (data.explosion && ret < set->DefaultExplosionDamage) {
            ret = set->DefaultExplosionDamage;
        }
        return ret;
    }

    RE::NiPoint3 GetWorldPosition(const FireVertex& vertex) {
        if (!vertex.cell) return RE::NiPoint3{0.0f, 0.0f, 0.0f};

        // Get cell grid coordinates
        auto coords = vertex.cell->GetCoordinates();
        int cellX = coords->cellX;
        int cellY = coords->cellY;

        // Cell world origin
        float cellWorldX = cellX * 4096.0f;
        float cellWorldY = cellY * 4096.0f;

        // Quadrant (qx, qy)
        int qx = vertex.quadrant % 2;
        int qy = vertex.quadrant / 2;

        // Quadrant world offset
        float quadWorldX = cellWorldX + qx * 2048.0f;
        float quadWorldY = cellWorldY + qy * 2048.0f;

        // Vertex (vx, vy) in quadrant
        int vx = vertex.vertex % 17;
        int vy = vertex.vertex / 17;

        // Vertex world offset
        float vertWorldX = quadWorldX + vx * 128.0f;
        float vertWorldY = quadWorldY + vy * 128.0f;

        float vertWorldZ = 0.0f;
        RE::TES::GetSingleton()->GetLandHeight(RE::NiPoint3{vertWorldX, vertWorldY, 0.0f}, vertWorldZ);

        return RE::NiPoint3{vertWorldX, vertWorldY, vertWorldZ};
    }

    std::tuple<bool, uint8_t, uint8_t> GetGrassData(RE::TESObjectLAND::LoadedLandData* loadedData, int q, int v) {
        auto set = Settings::GetSingleton();
        bool canBurn = false;
        uint8_t fuel = set->DefaultInitialFuelAmount;
        uint8_t minBurnHeat = set->DefaultMinHeatToBurn;
        bool defaultConfig = true;

        float defaultTexturePercent = 1.0f;

        for (int texIdx = 0; texIdx < 6; ++texIdx) {
            // Check percent coverage
            float percent = (static_cast<float>(loadedData->percents[q][v][texIdx])) / 255.0f;
            defaultTexturePercent -= percent;
            if (percent > 0.0f) {
                RE::TESLandTexture* tex = loadedData->quadTextures[q][texIdx];
                if (!tex) continue;

                // Check grass list
                for (const auto& grass : tex->textureGrassList) {
                    if (grass) {
                        // We dont have a name or EditorID for grass objects so we need to use model path :(
                        auto model = grass->As<RE::TESModel>();
                        auto modelPath = model ? model->model : "";
                        std::string modelString = modelPath.c_str();
                        for (const auto& entry : set->grassConfigs) {
                            if (modelString.find(entry.name) != std::string::npos) {
                                if (defaultConfig) {
                                    canBurn = entry.canBurn;
                                    fuel = entry.fuel;
                                    minBurnHeat = entry.minBurnHeat;
                                    defaultConfig = false;
                                } else {
                                    canBurn = canBurn || entry.canBurn;
                                    fuel = (fuel + entry.fuel) / 2;
                                    minBurnHeat = (minBurnHeat + entry.minBurnHeat) / 2;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (defaultTexturePercent > 0.0f) {
            RE::TESLandTexture* tex = loadedData->defQuadTextures[q];
            if (!tex) return {canBurn, fuel, minBurnHeat};

            // Check grass list
            for (const auto& grass : tex->textureGrassList) {
                if (grass) {
                    // We dont have a name or EditorID for grass objects so we need to use model path :(
                    auto model = grass->As<RE::TESModel>();
                    auto modelPath = model ? model->model : "";
                    std::string modelString = modelPath.c_str();
                    for (const auto& entry : set->grassConfigs) {
                        if (modelString.find(entry.name) != std::string::npos) {
                            if (defaultConfig) {
                                canBurn = entry.canBurn;
                                fuel = entry.fuel;
                                minBurnHeat = entry.minBurnHeat;
                                defaultConfig = false;
                            } else {
                                canBurn = canBurn || entry.canBurn;
                                fuel = (fuel + entry.fuel) / 2;
                                minBurnHeat = (minBurnHeat + entry.minBurnHeat) / 2;
                            }
                        }
                    }
                }
            }
        }

        return {canBurn, fuel, minBurnHeat};
    }

    std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }
    
    static bool MatchesAnyPattern(const std::string& lower, const std::vector<std::string>& patterns) {
        for (const auto& pat : patterns) {
            if (lower.find(pat) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    ProjectileType GetProjectileType(RE::Projectile* proj) {
        if (!proj) {
            return ProjectileType::Unknown;
        }

        auto set = Settings::GetSingleton();

        // 1. Source spell/magic item
        if (auto spellSource = proj->GetProjectileRuntimeData().spell) {
            if (auto editorID = spellSource->GetFormEditorID()) {
                std::string lowerID = ToLower(editorID);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ProjectileType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ProjectileType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ProjectileType::Water;
                }
            }
            if (auto fulName = spellSource->GetFullName()) {
                std::string lowerID = ToLower(fulName);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ProjectileType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ProjectileType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ProjectileType::Water;
                }
            }

            // Check base effects of the spell
            for (auto MagEf : spellSource->effects) {
                if (MagEf && MagEf->baseEffect) {
                    if (auto editorID = MagEf->baseEffect->GetFormEditorID()) {
                        std::string lowerID = ToLower(editorID);
                        if (MatchesAnyPattern(lowerID, set->fireSources)) {

                            return ProjectileType::Fire;
                        }
                        if (MatchesAnyPattern(lowerID, set->coldSources)) {
                            return ProjectileType::Cold;
                        }
                        if (MatchesAnyPattern(lowerID, set->waterSources)) {
                            return ProjectileType::Water;
                        }
                    }
                    if (auto fullName = MagEf->baseEffect->GetFullName()) {
                        std::string lowerID = ToLower(fullName);
                        if (MatchesAnyPattern(lowerID, set->fireSources)) {
                            return ProjectileType::Fire;
                        }
                        if (MatchesAnyPattern(lowerID, set->coldSources)) {
                            return ProjectileType::Cold;
                        }
                        if (MatchesAnyPattern(lowerID, set->waterSources)) {
                            return ProjectileType::Water;
                        }
                    }
                }
            }
        }

        // 2. Ammo source
        if (auto ammoSource = proj->GetProjectileRuntimeData().ammoSource) {
            if (auto editorID = ammoSource->GetFormEditorID()) {
                std::string lowerID = ToLower(editorID);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ProjectileType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ProjectileType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ProjectileType::Water;
                }
            }
            if (auto fullName = ammoSource->GetFullName()) {
                std::string lowerID = ToLower(fullName);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ProjectileType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ProjectileType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ProjectileType::Water;
                }
            }
        }

        // 3. Weapon source
        if (auto weapSource = proj->GetProjectileRuntimeData().weaponSource) {
            if (auto editorID = weapSource->GetFormEditorID()) {
                std::string lowerID = ToLower(editorID);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ProjectileType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ProjectileType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ProjectileType::Water;
                }
            }
            if (auto fullName = weapSource->GetFullName()) {
                std::string lowerID = ToLower(fullName);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ProjectileType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ProjectileType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ProjectileType::Water;
                }
            }
        }

        // 4. Projectile's own ID/name
        if (auto editorID = proj->GetFormEditorID()) {
            std::string lowerID = ToLower(editorID);
            if (MatchesAnyPattern(lowerID, set->fireSources)) {
                return ProjectileType::Fire;
            }
            if (MatchesAnyPattern(lowerID, set->coldSources)) {
                return ProjectileType::Cold;
            }
            if (MatchesAnyPattern(lowerID, set->waterSources)) {
                return ProjectileType::Water;
            }
        }
        return ProjectileType::Unknown;
    }

    ProjectileType GetExplosionType(RE::Explosion* exp) {
        if (!exp) {
            return ProjectileType::Unknown;
        }

        auto set = Settings::GetSingleton();

        if (auto base = exp->GetBaseObject()) {
            if (auto model = base->As<RE::TESModel>()) {
                std::string lowerID = ToLower(model->model.c_str());
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ProjectileType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ProjectileType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ProjectileType::Water;
                }
                logger::debug("Explosion model {}", model->model.c_str());
            }
        }
        return ProjectileType::Unknown;
    }

}  // namespace Utils