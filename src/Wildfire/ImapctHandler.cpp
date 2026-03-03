#include "Wildfire/Wildfire.h"

#include "Utils.h"
#include "Settings.h"

static float GetDamageFromProjectile(RE::Projectile* proj) {
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
                bool destruction = effect->baseEffect->GetMagickSkill() == RE::ActorValue::kDestruction;
                auto effectArchetype = effect->baseEffect->GetArchetype();
                bool archetype = effectArchetype == RE::EffectArchetypes::ArchetypeID::kValueModifier ||
                                 effectArchetype == RE::EffectArchetypes::ArchetypeID::kDualValueModifier;
                RE::TESObjectREFR* shooter = data.shooter.get().get();
                RE::TESObjectREFR* target = data.desiredTarget.get().get();
                bool conditionsTrue = effect->conditions.IsTrue(shooter, target) &&
                                      effect->baseEffect->conditions.IsTrue(shooter, target);
                logger::debug("shooter {}, target {}, conditionsTrue {}", shooter ? shooter->GetName() : "null",
                              target ? target->GetName() : "null", conditionsTrue);
                if (hostile && destruction && archetype && conditionsTrue) {
                    spellDamage += effect->GetMagnitude();
                }
            }
        }
        ret = spellDamage;
    }
    return ret;
}

void WildfireMgr::ProcessImpact(RE::Projectile* a_proj, RE::TESObjectREFR* a_ref, const RE::NiPoint3& pos) {
    auto base_damage = GetDamageFromProjectile(a_proj);

    static auto* set = Settings::GetSingleton();
    float adj_damage = base_damage;
    auto type = Utils::GetProjectileType(a_proj);
    switch (type) {
        case ImpactType::Fire:
            adj_damage = base_damage * set->FireDamageMultiplayer;
            break;
        case ImpactType::Shock:
            adj_damage = base_damage * set->ShockDamageMultiplayer;
            break;
        case ImpactType::Cold:
            adj_damage = base_damage * set->ColdDamageMultiplayer * (-1);
            break;
        case ImpactType::Water:
            adj_damage = base_damage * set->WaterDamageMultiplayer * (-1);
            break;
    }
    // ToDo: Shock can't heat if burning
    // ToDo: Water can't cool under 0 heat

    logger::debug("impact damage: {}", adj_damage);
    AddGrassEvent(pos, VERTS_STEP / 2, adj_damage, type);
    if (a_ref) {
        AddRefEvent(a_ref, adj_damage, type);
    }
}