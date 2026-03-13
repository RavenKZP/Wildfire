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
        if (refr) {
            return nullptr;
        }
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
        if (!obj) {
            return {0, 0, 0};
        }
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
        static auto* set = Settings::GetSingleton();
        auto base = ref->GetBaseObject();
        if (!base) {
            return false;
        }

        // 1. Exact FormID check (fast)
        auto fidIt = set->refsConfig.find(base->GetFormID());
        if (fidIt != set->refsConfig.end()) {
            logger::debug("Found ref in refsConfig");
            return fidIt->second.canBurn;
        }

        // 2. Trees always true without excluded
        if (base->Is(RE::FormType::Tree)) {
            auto fidIt = set->excludedTrees.find(base->GetFormID());
            if (fidIt != set->excludedTrees.end()) {
                logger::debug("Found tree in excludedTrees");
                return false;
            }
            return true;
        }
        return false;
    }

    bool IsOIFSupported(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }

        auto base = ref->GetBaseObject();
        if (!base) {
            return false;
        }

        using FT = RE::FormType;

        switch (base->GetFormType()) {
            case FT::Activator:
            case FT::TalkingActivator:
            case FT::Weapon:
            case FT::Armor:
            case FT::Ammo:
            case FT::Ingredient:
            case FT::Misc:
            case FT::KeyMaster:
            case FT::Book:
            case FT::Note:
            case FT::Scroll:
            case FT::SoulGem:
            case FT::AlchemyItem:  // potion + food
            case FT::Furniture:
            case FT::Door:
            case FT::Flora:
            case FT::Container:
            case FT::Static:
            case FT::MovableStatic:
            case FT::Tree:
            case FT::Light:
                return true;
            default:
                return false;
        }
    }

    bool IsPapyrusSupported(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }
        return true;
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

    ElementType GetProjectileType(RE::Projectile* proj) {
        static std::unordered_map<RE::TESForm*, ElementType> detectionCache;  // cache by base object
        static auto* set = Settings::GetSingleton();

        if (!proj) return ElementType::Neutral;

        auto* base = proj->GetBaseObject();
        if (!base) return ElementType::Neutral;

        // 1. Exact FormID check (fast)
        auto fidIt = set->sourceForms.find(base->GetFormID());
        if (fidIt != set->sourceForms.end()) {
            logger::debug("Found projectile in sourceForms");
            return fidIt->second;
        }

        // 2. Check cache by base object
        auto cacheIt = detectionCache.find(base);
        if (cacheIt != detectionCache.end()) {
            logger::debug("Found projectile in detectionCache");
            return cacheIt->second;
        }

        ElementType type = ElementType::Neutral;

        // Helper lambda for string pattern detection
        auto CheckPatterns = [&](RE::TESForm* form) -> ElementType {
            if (!form) return ElementType::Neutral;

            auto TryMatch = [&](const std::string& str) -> ElementType {
                std::string lowerStr = ToLower(str);
                if (MatchesAnyPattern(lowerStr, set->fireSources)) return ElementType::Fire;
                if (MatchesAnyPattern(lowerStr, set->shockSources)) return ElementType::Shock;
                if (MatchesAnyPattern(lowerStr, set->coldSources)) return ElementType::Cold;
                if (MatchesAnyPattern(lowerStr, set->waterSources)) return ElementType::Water;
                return ElementType::Neutral;
            };

            if (auto editorID = form->GetFormEditorID()) {
                type = TryMatch(editorID);
                if (type != ElementType::Neutral) return type;
            }
            if (auto fullName = form->GetName()) {
                type = TryMatch(fullName);
                if (type != ElementType::Neutral) return type;
            }

            return ElementType::Neutral;
        };

        // 3. Check spell source
        if (auto spellSource = proj->GetProjectileRuntimeData().spell) {
            type = CheckPatterns(spellSource);
            if (type != ElementType::Neutral) {
                detectionCache[base] = type;
                return type;
            }

            // Check base effects
            for (auto magEf : spellSource->effects) {
                if (magEf && magEf->baseEffect) {
                    type = CheckPatterns(magEf->baseEffect);
                    if (type != ElementType::Neutral) {
                        detectionCache[base] = type;
                        return type;
                    }
                }
            }
        }

        // 4. Ammo source
        type = CheckPatterns(proj->GetProjectileRuntimeData().ammoSource);
        if (type != ElementType::Neutral) {
            detectionCache[base] = type;
            return type;
        }

        // 5. Weapon source
        type = CheckPatterns(proj->GetProjectileRuntimeData().weaponSource);
        if (type != ElementType::Neutral) {
            detectionCache[base] = type;
            return type;
        }

        // 6. Projectile's own base object
        type = CheckPatterns(base);
        detectionCache[base] = type;
        return type;
    }

    ElementType GetExplosionType(RE::Explosion* exp) {
        static std::unordered_map<RE::TESForm*, ElementType> detectionCache;  // cache by base object
        static auto* set = Settings::GetSingleton();

        if (!exp) return ElementType::Neutral;

        auto* base = exp->GetBaseObject();
        if (!base) return ElementType::Neutral;

        // 1. Exact FormID lookup (fast)
        auto fidIt = set->sourceForms.find(base->GetFormID());
        if (fidIt != set->sourceForms.end()) {
            logger::debug("Found explosion in sourceForms");
            return fidIt->second;
        }

        // 2. Cache check
        auto cacheIt = detectionCache.find(base);
        if (cacheIt != detectionCache.end()) {
            logger::debug("Found explosion in detectionCache");
            return cacheIt->second;
        }

        ElementType type = ElementType::Neutral;

        // 3. Helper lambda for pattern matching
        auto CheckPatterns = [&](RE::TESForm* form) -> ElementType {
            if (!form) return ElementType::Neutral;

            auto TryMatch = [&](const std::string& str) -> ElementType {
                std::string lowerStr = ToLower(str);
                if (MatchesAnyPattern(lowerStr, set->fireSources)) return ElementType::Fire;
                if (MatchesAnyPattern(lowerStr, set->shockSources)) return ElementType::Shock;
                if (MatchesAnyPattern(lowerStr, set->coldSources)) return ElementType::Cold;
                if (MatchesAnyPattern(lowerStr, set->waterSources)) return ElementType::Water;
                return ElementType::Neutral;
            };

            if (auto editorID = form->GetFormEditorID()) {
                type = TryMatch(editorID);
                if (type != ElementType::Neutral) return type;
            }
            if (auto fullName = form->GetName()) {
                type = TryMatch(fullName);
                if (type != ElementType::Neutral) return type;
            }
            if (auto model = form->As<RE::TESModel>()) {
                type = TryMatch(model->model.c_str());
                if (type != ElementType::Neutral) return type;
            }

            return ElementType::Neutral;
        };

        type = CheckPatterns(base);
        detectionCache[base] = type;
        return type;
    }

    float GetDamageFromProjectile(RE::Projectile* proj) {
        if (!proj) {
            return 0.0f;
        }

        float ret = 0.0f;

        const auto& data = proj->GetProjectileRuntimeData();

        if (data.weaponSource) {
            if (data.weaponSource->attackDamage > 0.0f) {
                ret = static_cast<float>(data.weaponSource->attackDamage);
            }
        }

        if (data.ammoSource) {
            auto ammoRuntimeData = data.ammoSource->GetRuntimeData().data;
            if (ammoRuntimeData.damage > 0.0f) {
                ret = static_cast<float>(ammoRuntimeData.damage);
            }
        }

        if (data.spell) {
            float spellDamage = 0.0f;
            for (auto& effect : data.spell->effects) {
                if (effect) {
                    bool hostile = effect->IsHostile();
                    bool restoration = effect->baseEffect->GetMagickSkill() == RE::ActorValue::kRestoration;
                    bool illusion = effect->baseEffect->GetMagickSkill() == RE::ActorValue::kIllusion;

                    auto effectArchetype = effect->baseEffect->GetArchetype();
                    bool archetype = effectArchetype == RE::EffectArchetypes::ArchetypeID::kValueModifier ||
                                     effectArchetype == RE::EffectArchetypes::ArchetypeID::kDualValueModifier;
                    RE::TESObjectREFR* shooter = data.shooter.get().get();
                    RE::TESObjectREFR* target = data.desiredTarget.get().get();
                    bool conditionsTrue = effect->conditions.IsTrue(shooter, target) &&
                                          effect->baseEffect->conditions.IsTrue(shooter, target);
                    if (hostile && !restoration && !illusion && archetype && conditionsTrue) {
                        spellDamage += effect->GetMagnitude();
                    }
                }
            }
            ret = spellDamage;
        }
        return ret;
    }
}