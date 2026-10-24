#include "Utils.h"

#include "Math_Utils.h"
#include "Types.h"
#include "Settings.h"

#include "ClibUtil/editorID.hpp"

namespace Utils {

    float RandomFloat(float min, float max) {
        if (min > max) {
            std::swap(min, max);
        }
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng);
    }

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
    BoundaryBox GetBoundingBox(const RE::TESObjectREFR* a_obj) {
        /*
        static std::shared_mutex GeometryCacheMutex;
        static std::unordered_map<RE::FormID, std::pair<RE::NiPoint3, RE::NiPoint3>> minMaxCache;

        const auto center = GetPosition(a_obj);

        const auto baseObj = a_obj->GetBaseObject();
        RE::FormID modelFormID = baseObj ? baseObj->GetFormID() : a_obj->GetFormID();

        std::pair<RE::NiPoint3, RE::NiPoint3> min_max;

        std::shared_lock readLock(GeometryCacheMutex);
        auto itCache = minMaxCache.find(modelFormID);
        if (itCache != minMaxCache.end()) {
            min_max = itCache->second;
        } else {
            readLock.unlock();
            const Math::LinAlg::Geometry geometry(a_obj);
            std::unique_lock writeLock(GeometryCacheMutex);
            min_max = geometry.GetBoundingBox();
            minMaxCache[modelFormID] = min_max;
            writeLock.unlock();
        }
        auto [min, max] = min_max;
        */
        auto center = a_obj->GetPosition();
        auto min = a_obj->GetBoundMin();
        auto max = a_obj->GetBoundMax();

        RE::NiPoint3 obj_angle = a_obj->GetAngle();

        if (auto obj3D = a_obj->Get3D()) {
            auto modelAngle = obj3D->local.rotate;
            center = obj3D->world.translate;

            min = center + min;
            max = center + max;

            const auto v1 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, min.y, min.z) - center, modelAngle) + center;
            const auto v2 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, min.y, min.z) - center, modelAngle) + center;
            const auto v3 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, max.y, min.z) - center, modelAngle) + center;
            const auto v4 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, max.y, min.z) - center, modelAngle) + center;

            const auto v5 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, min.y, max.z) - center, modelAngle) + center;
            const auto v6 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, min.y, max.z) - center, modelAngle) + center;
            const auto v7 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, max.y, max.z) - center, modelAngle) + center;
            const auto v8 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, max.y, max.z) - center, modelAngle) + center;

            return {v1, v2, v3, v4, v5, v6, v7, v8};
        } else {
            min = center + min;
            max = center + max;

            const auto v1 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, min.y, min.z) - center, obj_angle) + center;
            const auto v2 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, min.y, min.z) - center, obj_angle) + center;
            const auto v3 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, max.y, min.z) - center, obj_angle) + center;
            const auto v4 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, max.y, min.z) - center, obj_angle) + center;

            const auto v5 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, min.y, max.z) - center, obj_angle) + center;
            const auto v6 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, min.y, max.z) - center, obj_angle) + center;
            const auto v7 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, max.y, max.z) - center, obj_angle) + center;
            const auto v8 =
                Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, max.y, max.z) - center, obj_angle) + center;

            return {v1, v2, v3, v4, v5, v6, v7, v8};
        }
    }

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
            return fidIt->second.canBurn;
        }

        // 2. Trees always true without excluded
        if (base->Is(RE::FormType::Tree)) {
            auto fidItExcluded = set->excludedTrees.find(base->GetFormID());
            if (fidItExcluded != set->excludedTrees.end()) {
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

    static RefType GetGeometryTypesRecursive(RE::NiAVObject* obj) {
        if (!obj) return RefType::Static;

        bool hasAlpha = false;
        bool hasNoAlpha = false;

        std::function<void(RE::NiAVObject*)> check = [&](RE::NiAVObject* node) {
            if (auto triShape = node->AsTriShape()) {
                auto& geomData = triShape->GetGeometryRuntimeData();
                auto& prop = geomData.alphaProperty;
                if (prop) {
                    hasAlpha = true;
                } else {
                    hasNoAlpha = true;
                }
            }
            if (node->AsNode()) {
                for (auto& child : node->AsNode()->GetChildren()) {
                    if (child) check(child.get());
                }
            }
        };

        check(obj);

        if (hasAlpha && hasNoAlpha) return RefType::Tree;
        if (hasAlpha) return RefType::Bush;
        return RefType::Static;
    }

    RefType GetGeometryType(RE::TESObjectREFR* ref) {
        RefType res = RefType::Static;
        if (!ref) {
            return res;
        }
        return GetGeometryTypesRecursive(ref->Get3D());
    }

    bool IsPapyrusSupported(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }
        return true;
    }
    
    std::string ElementTypeToStr(ElementType type) {
        switch (type) {
            case ElementType::Shock:
                return "Shock";
            case ElementType::Cold:
                return "Cold";
            case ElementType::Fire:
                return "Fire";
            case ElementType::Neutral:
                return "Neutral";
            case ElementType::Water:
                return "Water";
            default:
                return "Unknown";
        }
    }

    std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static bool MatchesAnyPattern(const std::string& lower, const std::vector<std::string>& patterns) {
        for (const auto& pat : patterns) {
            if (pat == "ice") {
                size_t pos = lower.find("ice");
                bool foundValidIce = false;

                while (pos != std::string::npos) {
                    // valid if NOT part of "voice"
                    if (!(pos >= 2 && lower[pos - 2] == 'v' && lower[pos - 1] == 'o')) {
                        foundValidIce = true;
                        break;
                    }
                    pos = lower.find("ice", pos + 1);
                }

                if (foundValidIce) return true;

            } else if (lower.find(pat) != std::string::npos) {
                return true;
            }
        }

        return false;
    }
    
     static ElementType TryMatch(const std::string& str) {
        static auto* set = Settings::GetSingleton();
        std::string lowerStr = ToLower(str);
        if (MatchesAnyPattern(lowerStr, set->neutralSources)) return ElementType::Neutral;
        if (MatchesAnyPattern(lowerStr, set->coldSources)) return ElementType::Cold;
        if (MatchesAnyPattern(lowerStr, set->fireSources)) return ElementType::Fire;
        if (MatchesAnyPattern(lowerStr, set->shockSources)) return ElementType::Shock;
        if (MatchesAnyPattern(lowerStr, set->waterSources)) return ElementType::Water;
        return ElementType::Unknown;
    };

    ElementType GetProjectileType(RE::Projectile* proj) {
        static auto* set = Settings::GetSingleton();
        static auto* DetectionCache = DetectionCache::GetSingleton();

        if (!proj) return ElementType::Unknown;

        auto* base = proj->GetBaseObject();
        if (!base) return ElementType::Unknown;

        auto baseFormID = base->GetFormID();

        // 1. Check cache by base object
        auto cacheIt = DetectionCache->Get(baseFormID);
        if (cacheIt.has_value()) {
            return cacheIt.value();
        }

        ElementType type = ElementType::Unknown;

        // Helper lambda for string pattern detection
        auto CheckPatterns = [&](RE::TESForm* form) -> ElementType {
            if (!form) return ElementType::Unknown;

            if (auto editorID = form->GetFormEditorID()) {
                type = TryMatch(editorID);
                if (type != ElementType::Unknown) return type;
            }
            auto editorID = clib_util::editorID::get_editorID(form);
            if (!editorID.empty()) {
                type = TryMatch(editorID);
                if (type != ElementType::Unknown) return type;
            }
            if (auto fullName = form->GetName()) {
                type = TryMatch(fullName);
                if (type != ElementType::Unknown) return type;
            }

            return ElementType::Unknown;
        };

        // 3. Check spell source
        if (auto spellSource = proj->GetProjectileRuntimeData().spell) {
            type = CheckPatterns(spellSource);
            if (type != ElementType::Unknown) {
                logger::debug("[Utils] Projectile {}, is {}", clib_util::editorID::get_editorID(base), ElementTypeToStr(type));
                DetectionCache->Add(baseFormID, type);
                return type;
            }

            // Check base effects
            for (auto magEf : spellSource->effects) {
                if (magEf && magEf->baseEffect) {
                    type = CheckPatterns(magEf->baseEffect);
                    if (type != ElementType::Unknown) {
                        logger::debug("[Utils] Projectile {}, is {}", clib_util::editorID::get_editorID(base),
                                      ElementTypeToStr(type));
                        DetectionCache->Add(baseFormID, type);
                        return type;
                    }
                }
            }
        }

        // 4. Ammo source
        type = CheckPatterns(proj->GetProjectileRuntimeData().ammoSource);
        if (type != ElementType::Unknown) {
            logger::debug("[Utils] Projectile {}, is {}", clib_util::editorID::get_editorID(base), ElementTypeToStr(type));
            DetectionCache->Add(baseFormID, type);
            return type;
        }

        // 5. Weapon source
        type = CheckPatterns(proj->GetProjectileRuntimeData().weaponSource);
        if (type != ElementType::Unknown) {
            logger::debug("[Utils] Projectile {}, is {}", clib_util::editorID::get_editorID(base), ElementTypeToStr(type));
            DetectionCache->Add(baseFormID, type);
            return type;
        }

        // 6. Projectile's own base object
        type = CheckPatterns(base);
        logger::debug("[Utils] Projectile {}, is {}", clib_util::editorID::get_editorID(base), ElementTypeToStr(type));
        DetectionCache->Add(baseFormID, type);
        return type;
    }

    ElementType GetExplosionType(RE::Explosion* exp) {
        static auto* set = Settings::GetSingleton();
        static auto* DetectionCache = DetectionCache::GetSingleton();

        if (!exp) return ElementType::Unknown;

        auto* base = exp->GetBaseObject();
        if (!base) return ElementType::Unknown;

        auto baseFormID = base->GetFormID();

        // 1. Cache check
        auto cacheIt = DetectionCache->Get(baseFormID);
        if (cacheIt.has_value()) {
            return cacheIt.value();
        }

        ElementType type = ElementType::Unknown;

        // Helper lambda for pattern matching
        auto CheckPatterns = [&](RE::TESForm* form) -> ElementType {
            if (!form) return ElementType::Unknown;

            if (auto editorID = form->GetFormEditorID()) {
                type = TryMatch(editorID);
                if (type != ElementType::Unknown) return type;
            }
            auto editorID = clib_util::editorID::get_editorID(form);
            if (!editorID.empty()) {
                type = TryMatch(editorID);
                if (type != ElementType::Unknown) return type;
            }
            if (auto fullName = form->GetName()) {
                type = TryMatch(fullName);
                if (type != ElementType::Unknown) return type;
            }
            if (auto model = form->As<RE::TESModel>()) {
                type = TryMatch(model->model.c_str());
                if (type != ElementType::Unknown) return type;
            }

            return ElementType::Unknown;
        };

        type = CheckPatterns(base);
        logger::debug("[Utils] Expolision {}, is {}", clib_util::editorID::get_editorID(base), ElementTypeToStr(type));
        DetectionCache->Add(baseFormID, type);
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

    RE::BSFixedString GetModelPath(RE::TESBoundObject* base) {
        if (!base) return "";

        if (auto activator = base->As<RE::TESObjectACTI>()) {
            return activator->model;
        }
        if (auto alch = base->As<RE::AlchemyItem>()) {
            return alch->model;
        }
        if (auto book = base->As<RE::TESObjectBOOK>()) {
            return book->model;
        }
        if (auto ingr = base->As<RE::IngredientItem>()) {
            return ingr->model;
        }
        if (auto light = base->As<RE::TESObjectLIGH>()) {
            return light->model;
        }
        if (auto misc = base->As<RE::TESObjectMISC>()) {
            return misc->model;
        }
        if (auto stat = base->As<RE::TESObjectSTAT>()) {
            return stat->model;
        }
        if (auto movStat = base->As<RE::BGSMovableStatic>()) {
            return movStat->model;
        }
        if (auto tree = base->As<RE::TESObjectTREE>()) {
            return tree->model;
        }
        if (auto flora = base->As<RE::TESFlora>()) {
            return flora->model;
        }
        if (auto furn = base->As<RE::TESFurniture>()) {
            return furn->model;
        }
        if (auto weap = base->As<RE::TESObjectWEAP>()) {
            return weap->model;
        }
        if (auto ammo = base->As<RE::TESAmmo>()) {
            return ammo->model;
        }
        if (auto cont = base->As<RE::TESObjectCONT>()) {
            return cont->model;
        }
        if (auto door = base->As<RE::TESObjectDOOR>()) {
            return door->model;
        }
    }
}