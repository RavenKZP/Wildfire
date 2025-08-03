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

        const auto& data = proj->GetProjectileRuntimeData();

        RE::Actor* actor = nullptr;
        if (data.shooter) {
            actor = data.shooter.get().get()->As<RE::Actor>();
        }

        if (data.spell) {
            if (actor) {
                return data.spell->CalculateMagickaCost(actor);
            } else {
                return data.spell->CalculateMagickaCost(nullptr);
            }
        }

        if (data.ammoSource) {
            return static_cast<float>(data.ammoSource->data.damage);
        }

        if (data.weaponSource) {
            return static_cast<float>(data.weaponSource->attackDamage);
        }

        return 0.0f;
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
        uint8_t fuel = set->InitialFuelAmount;
        uint8_t minBurnHeat = set->MinHeatToBurn;
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

    bool IsFireProjectile(RE::Projectile* proj) {
        if (!proj) return false;

        auto set = Settings::GetSingleton();

        auto checkString = [&](const char* str) -> bool {
            if (!str) return false;
            std::string lower = ToLower(str);
            for (const auto& pat : set->fireSources) {
                if (lower.find(pat) != std::string::npos) {
                    return true;
                }
            }
            return false;
        };

        // 1. Source spell/magic item
        if (auto spellSource = proj->GetProjectileRuntimeData().spell) {
            if (spellSource->GetFormEditorID() && checkString(spellSource->GetFormEditorID())) {
                logger::info("Found fire spell source: {}", spellSource->GetFormEditorID());
                return true;
            }
            if (spellSource->GetFullName() && checkString(spellSource->GetFullName())) {
                logger::info("Found fire spell source by name: {}", spellSource->GetFullName());
                return true;
            }
            logger::info("No fire spell match found for: {}",
                         spellSource->GetFormEditorID() ? spellSource->GetFormEditorID() : "Unknown");

            for (auto MagEf : spellSource->effects) {
                if (MagEf && MagEf->baseEffect && MagEf->baseEffect->GetFormEditorID() &&
                    checkString(MagEf->baseEffect->GetFormEditorID())) {
                    logger::info("Found fire effect: {}", MagEf->baseEffect->GetFormEditorID());
                    return true;
                }
                if (MagEf && MagEf->baseEffect && MagEf->baseEffect->GetFullName() &&
                    checkString(MagEf->baseEffect->GetFullName())) {
                    logger::info("Found fire effect by name: {}", MagEf->baseEffect->GetFullName());
                    return true;
                }
            }
            logger::info("No fire effect match found for spell: {}",
                         spellSource->GetFormEditorID() ? spellSource->GetFormEditorID() : "Unknown");
        }



        // 2. Projectile's own ID/name
        if (proj->GetFormEditorID() && checkString(proj->GetFormEditorID())) {
            logger::info("Found fire projectile: {}", proj->GetFormEditorID());
            return true;
        }

        logger::info("No fire projectile match found for: {}",
                     proj->GetFormEditorID() ? proj->GetFormEditorID() : "Unknown");
        return false;
    }

}  // namespace Utils