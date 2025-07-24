#include "Utils.h"	
#include "DrawDebug.h"

#include <unordered_set>

namespace Utils {

    void InitializeHazards() {
        fireHazard = RE::TESForm::LookupByEditorID("FireLgShortHazard")->As<RE::BGSHazard>();
    }

    bool new_grass_generated = false;

    const std::unordered_set<RE::FormType>& allowedTypes = {RE::FormType::Static, RE::FormType::MovableStatic,
                                                            RE::FormType::Flora, RE::FormType::Tree,
                                                            RE::FormType::Activator};

    std::vector<RE::TESObjectREFR*> GetObjectsInRadius(RE::TESObjectCELL* cell, const RE::NiPoint3& origin,
                                                       float radius) {
        std::vector<RE::TESObjectREFR*> results;

        cell->ForEachReferenceInRange(origin, radius, [&](RE::TESObjectREFR* ref) {
            if (!ref || ref->IsDisabled() || ref->IsDeleted() || ref->IsMarkedForDeletion()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            if (!allowedTypes.contains(ref->GetBaseObject()->GetFormType())) {
                logger::trace("Skipping object {} of type {}", ref->GetName(),
                              FormTypeToString(ref->GetBaseObject()->GetFormType()));
                return RE::BSContainer::ForEachResult::kContinue;
            } else {
                logger::trace("Processing object {} of type {}", ref->GetName(),
                              FormTypeToString(ref->GetBaseObject()->GetFormType()));
            }

            results.push_back(ref);

            return RE::BSContainer::ForEachResult::kContinue;
        });
        return results;
    }

    void GetCurrCellGrass() {
        RE::TESObjectCELL* targetCell = RE::PlayerCharacter::GetSingleton()->GetParentCell();

        RE::ExtraCellGrassData* CellGrassExtraData = targetCell->extraList.GetByType<RE::ExtraCellGrassData>();

        int count = 0;
        for (auto& extradata : targetCell->extraList) {
            if (extradata.GetType() == RE::ExtraDataType::kCellGrassData) {
                count++;
                logger::info("targetCell has {}, ExtraDataType::kCellGrassData", count);
            }
        }
        auto CellGrass = CellGrassExtraData->grassHandles;

        static auto grass_count = CellGrass.size();
        if (new_grass_generated) {
            logger::info("Getting Grass for cell, CellGrass: {}", CellGrass.size());
            grass_count = CellGrass.size();
            for (auto grass : CellGrass) {
               // grass->triShape->CullGeometry(true);
            }
            //targetCell->extraList.Remove(CellGrassExtraData);
        }
        
    }

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

}  // namespace Utils