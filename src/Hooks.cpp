#include "Hooks.h"
#include "DrawDebug.h"

#include <chrono>
#include <unordered_set>


namespace Hooks {

    const std::unordered_set<RE::FormType>& allowedTypes = {RE::FormType::Static, RE::FormType::MovableStatic,
                                                            RE::FormType::Flora, RE::FormType::Grass,
                                                            RE::FormType::Tree};

    std::vector<RE::TESObjectREFR*> GetObjectsInRadius(const RE::NiPoint3& origin, float radius) {
        std::vector<RE::TESObjectREFR*> results;

        auto* cell = RE::PlayerCharacter::GetSingleton()->GetParentCell();
        if (!cell) {
            return results;
        }

        float radiusSquared = radius * radius;

        cell->ForEachReference([&](RE::TESObjectREFR* ref) {
            if (!ref) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            /* IDK why it's not working
            if (!allowedTypes.contains(ref->GetFormType())) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            */

            float distSquared = ref->GetPosition().GetSquaredDistance(origin);
            if (distSquared <= radiusSquared) {
                results.push_back(ref);
            }

            return RE::BSContainer::ForEachResult::kContinue;
        });

        return results;
    }

    void Decal2::Install() {
        SKSE::AllocTrampoline(14);
        auto& trampoline = SKSE::GetTrampoline();
        originalFunction =
            trampoline.write_call<5>(REL::RelocationID(29250, 30104).address() + REL::Relocate(0x10, 0x10), thunk);
    }

    char Decal2::thunk(RE::BSTempEffectSimpleDecal* a1, RE::BSTriShape* a2) {

        logger::trace("d2 {} {} {}", a1->origin1.x, a1->origin1.y, a1->origin1.z);

        
        auto start = std::chrono::high_resolution_clock::now();
        RE::NiPoint3 pos = a1->origin1;
        auto objects = GetObjectsInRadius(pos, 100.0f);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        logger::info("Found {} objects in {:.3f} ms", objects.size(), duration.count());

        for (auto* obj : objects) {
            logger::info("Found object {} at distance {:.2f}", obj->GetName(), pos.GetDistance(obj->GetPosition()));

            auto objPos = obj->GetPosition();
            DebugAPI_IMPL::DebugAPI::DrawSphere(glm::vec3(objPos.x, objPos.y, objPos.z), 50, 5000,
                                                glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), 2.0f);
        }



        DebugAPI_IMPL::DebugAPI::DrawSphere(glm::vec3(a1->origin1.x, a1->origin1.y, a1->origin1.z), 50, 5000,
                                            glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);

        return originalFunction(a1, a2);
    }

    void UpdateHook::Update(RE::Actor* a_this, float a_delta) {
        Update_(a_this, a_delta);

        DebugAPI_IMPL::DebugAPI::Update();

    }

    void UpdateHook::Install() {

        REL::Relocation<std::uintptr_t> PlayerCharacterVtbl{RE::VTABLE_PlayerCharacter[0]};
        Update_ = PlayerCharacterVtbl.write_vfunc(0xAD, Update);

    }
}