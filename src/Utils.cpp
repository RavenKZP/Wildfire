#include "Utils.h"

#include "Math_Utils.h"
#include "Types.h"
#include "Settings.h"

namespace Utils {

    std::pair<int, int> GetCellCoords(RE::TESObjectCELL* cell) {
        if (!cell) {
            return {0, 0};  // Return default coordinates for null cell
        }
        auto coords = cell->GetCoordinates();
        return {coords->cellX, coords->cellY};
    }

    RE::TESObjectCELL* GetCellByCoords(int cellX, int cellY) {
        auto tes = RE::TES::GetSingleton();
        float worldX = cellX * float(CELL_SIZE);
        float worldY = cellY * float(CELL_SIZE);
        return tes->GetCell(RE::NiPoint3{worldX, worldY, 0.0f});
    }

    RE::TESObjectCELL* GetCellByCoords(const CellAsCoords& coords) {
        return GetCellByCoords(coords.cellX, coords.cellY);
    }

    // BOUNDARY BOX

    static RE::bhkRigidBody* GetRigidBody(const RE::TESObjectREFR* refr) {
        const auto object3D = refr->Get3D();
        if (!object3D) {
            return nullptr;
        }
        if (const auto body = object3D->GetCollisionObject()) {
            return body->GetRigidBody();
        }
        return nullptr;
    }

    BoundaryBox GetBoundingBox(const RE::TESObjectREFR* a_obj) {
        const auto center = GetPosition(a_obj);
        const Math::LinAlg::Geometry geometry(a_obj);
        auto [min, max] = geometry.GetBoundingBox();

        min = center + min;
        max = center + max;

        const auto obj_angle = a_obj->GetAngle();

        const auto v1 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, min.y, min.z) - center, obj_angle) + center;
        const auto v2 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, min.y, min.z) - center, obj_angle) + center;
        const auto v3 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, max.y, min.z) - center, obj_angle) + center;
        const auto v4 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, max.y, min.z) - center, obj_angle) + center;

        const auto v5 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, min.y, max.z) - center, obj_angle) + center;
        const auto v6 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, min.y, max.z) - center, obj_angle) + center;
        const auto v7 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, max.y, max.z) - center, obj_angle) + center;
        const auto v8 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, max.y, max.z) - center, obj_angle) + center;

        return {v1, v2, v3, v4, v5, v6, v7, v8};
    }

    RE::NiPoint3 GetPosition(const RE::TESObjectREFR* obj) {
        const auto body = GetRigidBody(obj);
        if (!body) return obj->GetPosition();
        RE::hkVector4 havockPosition;
        body->GetPosition(havockPosition);
        float components[4];
        _mm_store_ps(components, havockPosition.quad);
        RE::NiPoint3 newPosition = {components[0], components[1], components[2]};
        constexpr float havockToSkyrimConversionRate = 69.9915f;
        newPosition *= havockToSkyrimConversionRate;
        return newPosition;
    }

    bool IsSupportedForm(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }
        auto base = ref->GetBaseObject();
        if (!base) {
            return false;
        }
        // ToDo: Add exclude list from settings
        if (base->Is(RE::FormType::Tree)) {
            return true;
        }

        // ToDo: Add Include list from settings
        if (base->Is(RE::FormType::Flora) || base->Is(RE::FormType::Static) ||
            base->Is(RE::FormType::MovableStatic) || base->Is(RE::FormType::Activator) ||
            base->Is(RE::FormType::Container)) {
            return false;
        }
        return false;
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

    ImpactType GetProjectileType(RE::Projectile* proj) {
        if (!proj) {
            return ImpactType::Unknown;
        }

        auto* set = Settings::GetSingleton();

        // 1. Source spell/magic item
        if (auto spellSource = proj->GetProjectileRuntimeData().spell) {
            if (auto editorID = spellSource->GetFormEditorID()) {
                std::string lowerID = ToLower(editorID);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ImpactType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->shockSources)) {
                    return ImpactType::Shock;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ImpactType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ImpactType::Water;
                }
            }
            if (auto fulName = spellSource->GetFullName()) {
                std::string lowerID = ToLower(fulName);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ImpactType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->shockSources)) {
                    return ImpactType::Shock;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ImpactType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ImpactType::Water;
                }
            }

            // Check base effects of the spell
            for (auto MagEf : spellSource->effects) {
                if (MagEf && MagEf->baseEffect) {
                    if (auto editorID = MagEf->baseEffect->GetFormEditorID()) {
                        std::string lowerID = ToLower(editorID);
                        if (MatchesAnyPattern(lowerID, set->fireSources)) {
                            return ImpactType::Fire;
                        }
                        if (MatchesAnyPattern(lowerID, set->shockSources)) {
                            return ImpactType::Shock;
                        }
                        if (MatchesAnyPattern(lowerID, set->coldSources)) {
                            return ImpactType::Cold;
                        }
                        if (MatchesAnyPattern(lowerID, set->waterSources)) {
                            return ImpactType::Water;
                        }
                    }
                    if (auto fullName = MagEf->baseEffect->GetFullName()) {
                        std::string lowerID = ToLower(fullName);
                        if (MatchesAnyPattern(lowerID, set->fireSources)) {
                            return ImpactType::Fire;
                        }
                        if (MatchesAnyPattern(lowerID, set->shockSources)) {
                            return ImpactType::Shock;
                        }
                        if (MatchesAnyPattern(lowerID, set->coldSources)) {
                            return ImpactType::Cold;
                        }
                        if (MatchesAnyPattern(lowerID, set->waterSources)) {
                            return ImpactType::Water;
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
                    return ImpactType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->shockSources)) {
                    return ImpactType::Shock;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ImpactType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ImpactType::Water;
                }
            }
            if (auto fullName = ammoSource->GetFullName()) {
                std::string lowerID = ToLower(fullName);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ImpactType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->shockSources)) {
                    return ImpactType::Shock;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ImpactType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ImpactType::Water;
                }
            }
        }

        // 3. Weapon source
        if (auto weapSource = proj->GetProjectileRuntimeData().weaponSource) {
            if (auto editorID = weapSource->GetFormEditorID()) {
                std::string lowerID = ToLower(editorID);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ImpactType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->shockSources)) {
                    return ImpactType::Shock;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ImpactType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ImpactType::Water;
                }
            }
            if (auto fullName = weapSource->GetFullName()) {
                std::string lowerID = ToLower(fullName);
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ImpactType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->shockSources)) {
                    return ImpactType::Shock;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ImpactType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ImpactType::Water;
                }
            }
        }

        // 4. Projectile's own ID/name
        if (auto editorID = proj->GetFormEditorID()) {
            std::string lowerID = ToLower(editorID);
            if (MatchesAnyPattern(lowerID, set->fireSources)) {
                return ImpactType::Fire;
            }
            if (MatchesAnyPattern(lowerID, set->shockSources)) {
                return ImpactType::Shock;
            }
            if (MatchesAnyPattern(lowerID, set->coldSources)) {
                return ImpactType::Cold;
            }
            if (MatchesAnyPattern(lowerID, set->waterSources)) {
                return ImpactType::Water;
            }
        }
        return ImpactType::Unknown;
    }

    ImpactType GetExplosionType(RE::Explosion* exp) {
        if (!exp) {
            return ImpactType::Unknown;
        }

        auto set = Settings::GetSingleton();

        if (auto base = exp->GetBaseObject()) {
            if (auto model = base->As<RE::TESModel>()) {
                std::string lowerID = ToLower(model->model.c_str());
                if (MatchesAnyPattern(lowerID, set->fireSources)) {
                    return ImpactType::Fire;
                }
                if (MatchesAnyPattern(lowerID, set->shockSources)) {
                    return ImpactType::Shock;
                }
                if (MatchesAnyPattern(lowerID, set->coldSources)) {
                    return ImpactType::Cold;
                }
                if (MatchesAnyPattern(lowerID, set->waterSources)) {
                    return ImpactType::Water;
                }
            }
        }
        return ImpactType::Unknown;
    }
}