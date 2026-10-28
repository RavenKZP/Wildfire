#include "Wildfire/ReferenceUpdater.h"
#include "Wildfire/Wildfire.h"

#include "CLibUtilsQTR/Tasker.hpp"

#ifndef NDEBUG
#include "CLibUtilsQTR/DrawDebug.hpp"
#endif

#include "Settings.h"
#include "Utils.h"

#include <chrono>
#include <algorithm>
#include <random>

static void ReplaceBaseObject(RE::TESObjectREFR* ref, RE::TESBoundObject* newBase) {
    if (!ref || !newBase) {
        return;
    }
    ref->Disable();
    ref->Enable(false);
    ref->data.objectReference = newBase;
}

// Credits to Po3:
// https://github.com/powerof3/po3-Tweaks/blob/2eb5f73f6f9404c3e933e3b2aed1e5a518f559ab/src/Fixes/ToggleCollision.cpp#L23
static void ToggleRefCollision(RE::TESObjectREFR* a_ref, bool a_disable) {
    if (const auto root = a_ref->Get3D(); root) {
        const auto cell = a_ref->GetParentCell();

        if (const auto world = cell ? cell->GetbhkWorld() : nullptr) {
            RE::BSWriteLockGuard locker(world->worldLock);

            RE::BSVisit::TraverseScenegraphCollision(
                root, [&](RE::bhkNiCollisionObject* a_col) -> RE::BSVisit::BSVisitControl {
                    if (auto hkpBody = a_col->body
                                           ? static_cast<RE::hkpWorldObject*>(a_col->body->referencedObject.get())
                                           : nullptr;
                        hkpBody) {
                        hkpBody->collidable.broadPhaseHandle.collisionFilterInfo.SetNoCollision(a_disable);
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
        }
    }
}

static void SendFireEventsAcrossBounds(RefState refState, float wheatherFactor) {
    static auto TaskInterface = SKSE::GetTaskInterface();
    static auto set = Settings::GetSingleton();

    auto bounds = refState.BBox;

    float dx = (bounds[1] - bounds[0]).Length();
    float dy = (bounds[3] - bounds[0]).Length();
    float dz = (bounds[4] - bounds[0]).Length();

    dx = std::clamp(dx, 0.01f, 1000.0f);
    dy = std::clamp(dy, 0.01f, 1000.0f);
    dz = std::clamp(dz, 0.01f, 1000.0f);

    std::array<float, 3> dims = {dx, dy, dz};
    std::sort(dims.begin(), dims.end());
    float minDim = dims[0];
    float medDim = dims[1];
    float maxDim = dims[2];

    float radius = (minDim + medDim) / 2.0f;

    RE::NiPoint3 center{0, 0, 0};
    for (const auto& p : bounds) center = center + p;
    center *= (1.0f / 8.0f);

    RE::NiPoint3 dir;
    if (maxDim == dx)
        dir = (bounds[1] - bounds[0]);
    else if (maxDim == dy)
        dir = (bounds[3] - bounds[0]);
    else
        dir = (bounds[4] - bounds[0]);
    float len = dir.Length();
    if (len > 0.0f) dir *= (1.0f / len);

    int steps = static_cast<int>(maxDim / radius);
    if (steps < 1) steps = 1;

    RE::NiPoint3 start = center - dir * (maxDim / 2.0f);

    float damage = refState.heat * 0.25f * wheatherFactor;
    auto refPos = refState.position;
    for (int i = 0; i <= steps; ++i) {
        RE::NiPoint3 pos = start + dir * (i * radius);
        TaskInterface->AddTask([pos, radius, damage, refPos]() {
            auto scaledDamage = damage * set->FireDamageMultiplayer;
            auto* WildfireMgr = WildfireMgr::GetSingleton();
            WildfireMgr->AddGrassEvent(pos, radius, scaledDamage, ElementType::Fire);

            RE::TES::GetSingleton()->ForEachReferenceInRange(
                pos, radius, [damage, pos, radius, refPos, WildfireMgr](RE::TESObjectREFR* ref) {
                    if (ref) {
                        if (Utils::IsSupportedForm(ref) && ref->GetPosition() != refPos) {
                            auto newRefPos = ref->GetPosition();
                            auto distance = pos.GetDistance(newRefPos);
                            float adjusted_damage = damage * (distance / radius) * set->FireDamageMultiplayer;
                            WildfireMgr->AddRefEvent(ref, adjusted_damage, ElementType::Fire);
                            WildfireMgr->SendHitEvent(ref);
                        }
                    }

                    return RE::BSContainer::ForEachResult::kContinue;
                });
        });
    }
}

static void CullAlphaGeometryRecursive(RE::NiAVObject* obj) {
    if (!obj) return;

    if (auto triShape = obj->AsTriShape()) {
        auto& geomData = triShape->GetGeometryRuntimeData();
        auto& prop = geomData.alphaProperty;
        if (prop) {
            triShape->CullGeometry(true);
        }
    }

    if (obj->AsNode()) {
        for (auto& child : obj->AsNode()->GetChildren()) {
            if (child) CullAlphaGeometryRecursive(child.get());
        }
    }
}

static void CullNonAlphaGeometryRecursive(RE::NiAVObject* obj) {
    if (!obj) return;

    if (auto triShape = obj->AsTriShape()) {
        auto& geomData = triShape->GetGeometryRuntimeData();
        auto& prop = geomData.alphaProperty;
        if (!prop) {
            triShape->CullGeometry(true);
        }
    }

    if (obj->AsNode()) {
        for (auto& child : obj->AsNode()->GetChildren()) {
            if (child) CullAlphaGeometryRecursive(child.get());
        }
    }
}

static void FadeAlphaGeometryRecursive(RE::NiAVObject* obj) {
    if (!obj) return;

    if (auto triShape = obj->AsTriShape()) {
        auto& geomData = triShape->GetGeometryRuntimeData();
        auto& prop = geomData.alphaProperty;
        if (prop) {
            triShape->GetFadeAmount() = 0.5f;
        }
    }

    if (obj->AsNode()) {
        for (auto& child : obj->AsNode()->GetChildren()) {
            if (child) FadeAlphaGeometryRecursive(child.get());
        }
    }
}

static void DrawObjectBoundsBox(RE::TESObjectREFR* ref, int lifetimeMS, const RE::NiColorA& color, float thickness) {
    if (!ref) return;

#ifndef NDEBUG
    static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();

    // Local bounds
    auto points = Utils::GetBoundingBox(ref);

    RE::NiPoint3 p000 = {points[0].x, points[0].y, points[0].z};
    RE::NiPoint3 p100 = {points[1].x, points[1].y, points[1].z};
    RE::NiPoint3 p110 = {points[2].x, points[2].y, points[2].z};
    RE::NiPoint3 p010 = {points[3].x, points[3].y, points[3].z};
    RE::NiPoint3 p001 = {points[4].x, points[4].y, points[4].z};
    RE::NiPoint3 p101 = {points[5].x, points[5].y, points[5].z};
    RE::NiPoint3 p111 = {points[6].x, points[6].y, points[6].z};
    RE::NiPoint3 p011 = {points[7].x, points[7].y, points[7].z};

    // Bottom face (min.z)
    DebugAPI->DrawLineForMS(p000, p100, lifetimeMS, color, thickness);
    DebugAPI->DrawLineForMS(p100, p110, lifetimeMS, color, thickness);
    DebugAPI->DrawLineForMS(p110, p010, lifetimeMS, color, thickness);
    DebugAPI->DrawLineForMS(p010, p000, lifetimeMS, color, thickness);

    // Top face (max.z)
    DebugAPI->DrawLineForMS(p001, p101, lifetimeMS, color, thickness);
    DebugAPI->DrawLineForMS(p101, p111, lifetimeMS, color, thickness);
    DebugAPI->DrawLineForMS(p111, p011, lifetimeMS, color, thickness);
    DebugAPI->DrawLineForMS(p011, p001, lifetimeMS, color, thickness);

    // Vertical edges
    DebugAPI->DrawLineForMS(p000, p001, lifetimeMS, color, thickness);
    DebugAPI->DrawLineForMS(p100, p101, lifetimeMS, color, thickness);
    DebugAPI->DrawLineForMS(p110, p111, lifetimeMS, color, thickness);
    DebugAPI->DrawLineForMS(p010, p011, lifetimeMS, color, thickness);
#endif
}

ReferenceUpdater::ReferenceUpdater() : running_(false) {
    CharredFXS = RE::TESForm::LookupByEditorID<RE::TESEffectShader>("Wildfire_CharredFXS");
    CharredInstantFXS = RE::TESForm::LookupByEditorID<RE::TESEffectShader>("Wildfire_CharredInstantFXS");
    BurningFXS = RE::TESForm::LookupByEditorID<RE::TESEffectShader>("Wildfire_BurningFXS");
    FrostFXS = RE::TESForm::LookupByEditorID<RE::TESEffectShader>("Wildfire_FrostFXS");
    EmptyFXS = RE::TESForm::LookupByEditorID<RE::TESEffectShader>("Wildfire_EmptyFXS");
    DesintegrateFXS = RE::TESForm::LookupByEditorID<RE::TESEffectShader>("Wildfire_DesintegrateFXS");

    const char* BranchesIds[] = {"Wildfire_BranchPile01", "Wildfire_BranchPile02"};

    for (auto id : BranchesIds) {
        if (auto* Branch = RE::TESForm::LookupByEditorID<RE::TESBoundObject>(id)) {
            DefaultBurnedBranches.push_back(Branch);
        } else {
            logger::warn("[ReferenceUpdater] Burned Branches '{}' not found, skipping.", id);
        }
    }

    const char* dummyIds[] = {"Wildfire_DummyHazard01", "Wildfire_DummyHazard02", "Wildfire_DummyHazard03",
                              "Wildfire_DummyHazard04", "Wildfire_DummyHazard05", "Wildfire_DummyHazard06",
                              "Wildfire_DummyHazard07", "Wildfire_DummyHazard08", "Wildfire_DummyHazard09"};

    for (auto id : dummyIds) {
        if (auto* hazard = RE::TESForm::LookupByEditorID<RE::BGSHazard>(id)) {
            DummyHazards.push_back(hazard);
        } else {
            logger::warn("[ReferenceUpdater] Dummy hazard '{}' not found, skipping.", id);
        }
    }

    auto* set = Settings::GetSingleton();
    if (CharredFXS && CharredInstantFXS && BurningFXS && FrostFXS && EmptyFXS && DesintegrateFXS) {
        logger::info("[ReferenceUpdater] Effect shaders forms found and loaded.");
    } else {
        set->ModActive = false;
        logger::error("[ReferenceUpdater] Effect shaders forms not found. Please make sure Wildfire.esp is active");
        RE::DebugMessageBox("Wildfire Error: Effect shaders forms not found. Please make sure Wildfire.esp is active");
    }

    if (!DefaultBurnedBranches.empty()) {
        logger::info("[ReferenceUpdater] Burned branches forms found and loaded. (count={}).", DefaultBurnedBranches.size());
    } else {
        set->ModActive = false;
        logger::error("[ReferenceUpdater] Burned branches forms not found. Please make sure Wildfire.esp is active");
        RE::DebugMessageBox("Wildfire Error: Burned branches forms not found. Please make sure Wildfire.esp is active");
    }

    if (!DummyHazards.empty()) {
        logger::info("[ReferenceUpdater] Dummy Hazard(s) found and loaded (count={}).", DummyHazards.size());
    } else {
        set->ModActive = false;
        logger::error("[ReferenceUpdater] No Dummy Hazards found. Please make sure Wildfire.esp is active");
        RE::DebugMessageBox("Wildfire Error: Dummy Hazard not found. Please make sure Wildfire.esp is active");
    }
}

void ReferenceUpdater::OnGameLoad() {
    ClearResult();
    std::queue<RE::ObjectRefHandle> empty;
    {
        std::lock_guard lock(createRefStateMutex);
        createRefStateQueue.swap(empty);
    }
    /*
    MeltingObjects.clear();
    FadingObjects.clear();
    */
}

void ReferenceUpdater::HazardLoaded(RE::TESObjectREFR* ref) {
    if (!ref) {
        return;
    }
    TransformingData data;
    data.refHandle = ref->GetHandle();
    FadingObjects.push_back(data);
}

void ReferenceUpdater::RefLoaded(RE::TESObjectREFR* ref, RefState state) {
    if (state.isCharred == true) {
        if (state.type == RefType::Bush) {
            int random = rand() % DefaultBurnedBranches.size();
            RE::TESBoundObject* newBase = DefaultBurnedBranches[random];
            ReplaceBaseObject(ref, newBase);
        }
        switch (state.type) {
            case RefType::Static:
                ApplyCharredInstantFXS(ref);
                break;
            case RefType::Tree:
                RemoveLeaves(ref);
                ApplyCharredInstantFXS(ref);
                break;
            case RefType::Bush:
                break;
            case RefType::FloraCoveredUnburnable:
                RemoveLeaves(ref);
                break;
            case RefType::BaseObjSwap:
                ReplaceBaseObject(ref, state.baseObjectSwap);
                break;
            case RefType::Disintegrate:
                ReplaceBaseObject(ref, state.baseObjectSwap);
                break;
            case RefType::Melt:
                RemoveCollision(ref);
                TransformingData data;
                data.refHandle = ref->GetHandle();
                data.baseObjectSwap = state.baseObjectSwap;
                data.Progress = 0.0f;
                MeltingObjects.push_back(data);
                break;
        }
    }
}

bool ReferenceUpdater::StartUpdate(RefEventType& refEvents, RefStateMapType& burningRefs, WeatherData& wheatherData,
                                   float delta) {

    std::queue<RE::ObjectRefHandle> objToRemove;
    static auto* set = Settings::GetSingleton();
    static auto* tes = RE::TES::GetSingleton();
    for (auto& [refHandle, state] : burningRefs) {
        auto ref = refHandle.get().get();
        if (!ref) {
            objToRemove.push(refHandle);
            continue;
        } else if (ref->IsDisabled() || ref->IsDeleted()) {
            objToRemove.push(refHandle);
            continue;
        }
        auto currPos = Utils::GetPosition(ref); // Check if position was changed
        if (currPos != state.position) {
            state.position = currPos;
            state.BBox = Utils::GetBoundingBox(ref);

            auto* cell = ref->GetParentCell();
            if (cell) {
                float minZ = state.BBox[0].z;
                float maxZ = state.BBox[0].z;

                for (const auto& v : state.BBox) {
                    minZ = std::min(minZ, v.z);
                    maxZ = std::max(maxZ, v.z);
                }
                auto waterHeight = tes->GetWaterHeight(state.position, cell);

                float totalHeight = maxZ - minZ;
                float aboveWaterHeight = maxZ - std::max(minZ, waterHeight);

                float ratio = aboveWaterHeight / totalHeight;

                if (ratio <= set->UnderwaterRatio) {
                    state.isUnderwater = true;
                } else {
                    state.isUnderwater = false;
                }
            }
        }
        if (set->DebugMode && set->DrawBoundBoxes) {
            if (ref->GetParentCell() == RE::PlayerCharacter::GetSingleton()->GetParentCell()) {
                RE::NiColorA colour{0, 1, 0, 1};  // Green

                if (state.isBurning)
                    colour = {1, 0, 0, 1};
                else if (state.isCharred)
                    colour = {0, 0, 0, 1};
                else if (state.isUnderwater)
                    colour = {0, 0, 1, 1};
                else if (!state.canBurn)
                    colour = {0.75f, 0.75f, 0.75f, 1};
                else if (state.heat > 0)
                    colour = {1, 1, 0, 1};
                else if (state.heat < 0)
                    colour = {0, 1, 1, 1};
                DrawObjectBoundsBox(ref, set->DebugDrawDuration, colour, 1);
            }
        }
    }
    while (!objToRemove.empty()) {
        burningRefs.erase(objToRemove.front());
        objToRemove.pop();
    }

    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return false;  // Already running
    }

    std::queue<RE::ObjectRefHandle> createRefState;
    {
        std::unique_lock lock(createRefStateMutex);
        createRefState.swap(createRefStateQueue);
    }
    while (!createRefState.empty()) {
        RE::ObjectRefHandle refHandle = createRefState.front();
        if (refHandle.get().get()) {
            GetOrCreateRefState(burningRefs, refHandle);
        }
        createRefState.pop();
    }

    clib_utilsQTR::Tasker::GetSingleton()->PushTask(
        [this, refEvents, burningRefs, wheatherData, delta]() {
            RefStateMapType result = ReferenceUpdateLogic(refEvents, burningRefs, wheatherData, delta);
            {
                std::lock_guard lock(mutex_);
                result_ = result;
            }
            running_.store(false);
        },
        0);
    return true;
}

bool ReferenceUpdater::IsRunning() const { return running_.load(); }

std::optional<RefStateMapType> ReferenceUpdater::GetResult() {
    std::lock_guard lock(mutex_);
    return result_;
}

void ReferenceUpdater::ClearResult() {
    std::lock_guard lock(mutex_);
    result_.reset();
}


void ReferenceUpdater::TransformObjects(float delta) {
    auto start = std::chrono::high_resolution_clock::now();
    static auto* set = Settings::GetSingleton();

    if (MeltingObjects.empty() && FadingObjects.empty()) {
        return;
    }

    constexpr float kDurationSeconds = 5.0f;
    const float decayPerSec = 1.0f / kDurationSeconds;
    const float decay = decayPerSec * delta;

    // Process melting objects
    for (auto it = MeltingObjects.begin(); it != MeltingObjects.end();) {
        auto handle = it->refHandle;
        auto baseObjectSwap = it->baseObjectSwap;
        auto& Progress = it->Progress;

        auto* ref = handle.get().get();
        if (!ref || ref->IsDisabled() || ref->IsDeleted()) {
            it = MeltingObjects.erase(it);
            continue;
        }

        Progress = std::max(0.0f, Progress - decay);
        MeltObject(ref, baseObjectSwap, Progress);

        if (Progress <= 0.0f) {
            it = MeltingObjects.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = FadingObjects.begin(); it != FadingObjects.end();) {
        auto handle = it->refHandle;
        auto baseObjectSwap = it->baseObjectSwap;
        auto& Progress = it->Progress;

        auto* ref = handle.get().get();
        if (!ref || ref->IsDisabled() || ref->IsDeleted()) {
            it = FadingObjects.erase(it);
            continue;
        }

        Progress = std::max(0.0f, Progress - decay);

        FadeOutObject(ref, baseObjectSwap, Progress);

        if (Progress <= 0.0f) {
            it = FadingObjects.erase(it);
        } else {
            ++it;
        }
    }

    if (set->EnableTimeLogging) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        logger::debug("[Main][ReferenceUpdater] Transform {} Objects in {} ms", MeltingObjects.size() + FadingObjects.size(), elapsed.count());
    }
}

RefStateMapType ReferenceUpdater::ReferenceUpdateLogic(RefEventType refEvents, RefStateMapType burningRefs,
                                                       WeatherData wheatherData, float delta) {
    auto start = std::chrono::high_resolution_clock::now();
    static auto* set = Settings::GetSingleton();

    if (set->EnableTimeLogging) {
        logger::info("[Thread][ReferenceUpdater] Starting refs update with {} refEvents, {} burningRefs and {} delta",
                     refEvents.size(), burningRefs.size(), delta);
    }

    auto* TaskInterface = SKSE::GetTaskInterface();
    // Step 1 : Process each fire event and update the corresponding refs in the burningRefs
    for (const auto& event : refEvents) {
        if (event.type == ElementType::Neutral) {
            continue;  // Shouldn't happen but to be sure :)
        }
        auto refHandle = event.refHandle;
        auto damage = event.damage;
        auto type = event.type;
        if (refHandle) {
            auto refState = GetRefState(burningRefs, refHandle);
            if (!refState) {
                std::unique_lock lock(createRefStateMutex);
                createRefStateQueue.push(refHandle);
                continue;
            }
            if (!refState->isCharred) {
                if (type == ElementType::Fire || type == ElementType::Shock) {
                    damage = std::max(0.0f, damage);  // can't cool
                }

                if (type == ElementType::Water || type == ElementType::Cold) {
                    damage = std::min(0.0f, damage);  // can't heat
                }

                float newHeat = refState->heat + damage;

                if (type == ElementType::Shock && refState->heat < refState->ignitionHeat) {
                    refState->heat = std::min(newHeat, refState->ignitionHeat);
                } else if (type == ElementType::Water) {
                    refState->heat = std::max(newHeat, 0.0f);
                } else {
                    refState->heat = std::min(newHeat, std::max(refState->ignitionHeat, set->DefaultMaxRefHeat));
                }

                if (refState->heat >= refState->ignitionHeat && refState->canBurn && !refState->isBurning &&
                    !refState->isUnderwater) {
                    refState->isFrozen = false;
                    refState->isBurning = true;
                    TaskInterface->AddTask([this, refHandle]() {
                        if (auto ref = refHandle.get().get()) {
                            ApplyBurningFX(ref);
                            ref->SetActivationBlocked(true);
                        }
                    });
                }
                if (refState->heat <= -refState->ignitionHeat) {
                    refState->heat = -refState->ignitionHeat;
                    if (!refState->isFrozen) {
                        refState->isFrozen = true;
                        refState->isBurning = false;
                        TaskInterface->AddTask([this, refHandle]() {
                            if (auto ref = refHandle.get().get()) {
                                ApplyFrostFX(ref);
                                ref->SetActivationBlocked(false);
                            }
                            });
                    }
                }
                if (refState->heat <= 0.0f && refState->isBurning) {
                    refState->isBurning = false;
                    TaskInterface->AddTask([this, refHandle]() {
                        if (auto ref = refHandle.get().get()) {
                            RemoveFX(ref);
                            ref->SetActivationBlocked(false);
                        }
                        });
                }
                if (refState->heat >= 0.0f && refState->isFrozen) {
                    refState->isFrozen = false;
                    TaskInterface->AddTask([this, refHandle]() {
                        if (auto ref = refHandle.get().get()) {
                            RemoveFX(ref);
                        }
                    });
                }
            }
        }
    }
    if (set->EnableTimeLogging) {
        auto afterEvents = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = afterEvents - start;
        logger::info("[Thread][ReferenceUpdater] Processed {} fire refEvents in {} ms", refEvents.size(), elapsed.count());
    }

    // Step 2 : Simulate fire and cooling for each ref in the burningRefs
    float RainingFactor = wheatherData.isRaining ? set->RainingFactor : 1.0f;
    float SnowingFactor = wheatherData.isSnowing ? set->SnowingFactor : 1.0f;
    float wheatherFactor = RainingFactor * SnowingFactor;
    for (auto& [refHandle, state] : burningRefs) {
        if (state.isCharred) {
            continue;
        } else if (state.isBurning) {
            // allow underwater "burn" if not actual burn
            if (state.isUnderwater && state.initialFuel > 0.0f) {
                state.isBurning = false;
                state.heat = 0.0f;

                TaskInterface->AddTask([this, refHandle]() {
                    if (auto ref = refHandle.get().get()) {
                        RemoveFX(ref);
                        ref->SetActivationBlocked(false);
                    }
                });
                continue;
            }

            state.heat =
                std::min(state.heat + (set->FuelConsumptionRate * set->FuelToHeatRate * delta), set->DefaultMaxRefHeat);
            state.fuel -= set->FuelConsumptionRate * delta;

            if (state.initialFuel > 0.0f && state.fuel < state.initialFuel * 0.5f && state.halfEvent == false) {
                state.halfEvent = true;
                SendFireEventsAcrossBounds(state, wheatherFactor);
                state.heat *= 0.5;
            }

            if (state.fuel <= 0.0f) {
                state.isBurning = false;
                state.isCharred = true;
                if (state.initialFuel > 0.0f) {
                    SendFireEventsAcrossBounds(state, wheatherFactor);
                }
                switch (state.type) {
                    case RefType::Static:
                        TaskInterface->AddTask([this, refHandle]() {
                            if (auto ref = refHandle.get().get()) {
                                ApplyCharredFX(ref);
                                SpawnSmokeAndDecal(ref);
                            }
                        });
                        break;
                    case RefType::Tree:
                        TaskInterface->AddTask([this, refHandle]() {
                            if (auto ref = refHandle.get().get()) {
                                RemoveLeavesSmooth(ref);
                                ApplyCharredFX(ref);
                                // Remove Tree Animation
                                SpawnSmokeAndDecal(ref);
                            }
                        });
                        break;
                    case RefType::Bush:
                        TaskInterface->AddTask([this, refHandle]() {
                            if (auto ref = refHandle.get().get()) {
                                int random = rand() % DefaultBurnedBranches.size();
                                RE::TESBoundObject* newBase = DefaultBurnedBranches[random];
                                //ApplyDesintegrateFX(refHandle, newBase);
                                SpawnSmokeAndDecal(ref);
                                TransformingData data;
                                data.refHandle = refHandle;
                                data.baseObjectSwap = newBase;
                                FadingObjects.push_back(data);
                            }
                        });
                        break;
                    case RefType::FloraCoveredUnburnable:
                         TaskInterface->AddTask([this, refHandle]() {
                            if (auto ref = refHandle.get().get()) {
                                RemoveFX(ref);
                                RemoveLeavesSmooth(ref);
                                SpawnSmokeAndDecal(ref);
                            }
                        });
                        break;
                    case RefType::BaseObjSwap:
                        TaskInterface->AddTask([this, refHandle, baseObjectSwap = state.baseObjectSwap]() {
                            if (auto ref = refHandle.get().get()) {
                                RemoveFX(ref);
                                ReplaceBaseObject(ref, baseObjectSwap);
                                SpawnSmokeAndDecal(ref);
                            }
                        });
                        break;
                    case RefType::Disintegrate:
                        TaskInterface->AddTask([this, refHandle, baseObjectSwap = state.baseObjectSwap]() {
                            if (auto ref = refHandle.get().get()) {
                                //ApplyDesintegrateFX(refHandle, baseObjectSwap);RemoveCollision(ref);
                                TransformingData data;
                                data.refHandle = refHandle;
                                data.baseObjectSwap = baseObjectSwap;
                                FadingObjects.push_back(data);
                                SpawnSmokeAndDecal(ref);
                            }
                        });
                        break;
                    case RefType::Melt:
                        TaskInterface->AddTask([this, refHandle, baseObjectSwap = state.baseObjectSwap]() {
                            if (auto ref = refHandle.get().get()) {
                                RemoveFX(ref);
                                RemoveCollision(ref);
                                TransformingData data;
                                data.refHandle = refHandle;
                                data.baseObjectSwap = baseObjectSwap;
                                MeltingObjects.push_back(data);
                            }
                        });
                        break;
                }
            }
        } else if (state.heat > 0.0f) {
            // Cool down the ref if not burning
            state.heat -= set->SelfHeatLoss * delta;
            if (state.heat <= 0.0f) {
                state.heat = 0.0f;
                TaskInterface->AddTask([this, refHandle]() {
                    if (auto ref = refHandle.get().get()) {
                        RemoveFX(ref);
                    }
                    });
            }
        } else if (state.heat < 0.0f) {
            // Heat up the ref if below zero
            state.heat += set->SelfColdLoss * delta;
            if (state.heat >= 0.0f) {
                state.heat = 0.0f;
                state.isFrozen = false;
                TaskInterface->AddTask([this, refHandle]() {
                    if (auto ref = refHandle.get().get()) {
                        RemoveFX(ref);
                    }
                });
            }
        } else if (state.fuel < state.initialFuel) {
            state.fuel += set->FuelRecoveryRate * delta;
            if (state.fuel > state.initialFuel) {
                state.fuel = state.initialFuel;
            }
        }
    }
    if (set->EnableTimeLogging) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        logger::debug("[Thread][ReferenceUpdater] ReferenceUpdater: processed in {} ms", elapsed.count());
    }
    return burningRefs;
}

RefState* ReferenceUpdater::GetOrCreateRefState(RefStateMapType& burningRefs, RE::ObjectRefHandle refHandle) {
    auto ref = refHandle.get().get();
    if (!ref) {
        return nullptr;
    }
    auto it = burningRefs.find(refHandle);
    if (it != burningRefs.end()) {
        return &it->second;  // Already exists, return it
    }
    auto [newIt, _] = burningRefs.emplace(refHandle, RefState(ref));
    return &newIt->second;
}

RefState* ReferenceUpdater::GetRefState(RefStateMapType& burningRefs, RE::ObjectRefHandle refHandle) {
    if (!refHandle) {
        return nullptr;
    }
    auto it = burningRefs.find(refHandle);
    if (it != burningRefs.end()) {
        return &it->second;
    }
    return nullptr;
}


void ReferenceUpdater::ResetAllObjects(RefStateMapType& burningRefs) {
    for (auto& [refHandle, state] : burningRefs) {
        if (auto ref = refHandle.get().get()) {
            if (auto ref3D = ref->Get3D()) {
                ref3D->CullGeometry(false);
            }
            ref->Disable();
            ref->Enable(false);
            if (state.originalBaseObj) {
                ref->data.objectReference = state.originalBaseObj;
            }
        }
    }

}

void ReferenceUpdater::ApplyBurningFX(RE::TESObjectREFR* ref) {
    if (!ref) return;
    //ref->ApplyEffectShader(BurningFXS);
}

void ReferenceUpdater::ApplyCharredFX(RE::TESObjectREFR* ref) {
    if (!ref) return;
    ref->ApplyEffectShader(CharredFXS);
}

void ReferenceUpdater::ApplyCharredInstantFXS(RE::TESObjectREFR* ref) {
    if (!ref) return;
    ref->ApplyEffectShader(CharredInstantFXS);
}

void ReferenceUpdater::ApplyFrostFX(RE::TESObjectREFR* ref) {
    if (!ref) return;
    ref->ApplyEffectShader(FrostFXS);
}

void ReferenceUpdater::ApplyDesintegrateFX(RE::ObjectRefHandle refHandle, RE::TESBoundObject* baseObjectSwap) {
    if (auto ref = refHandle.get().get()) {
        ref->ApplyEffectShader(DesintegrateFXS);
        clib_utilsQTR::Tasker::GetSingleton()->PushTask(
            [this, refHandle, baseObjectSwap]() {
                SKSE::GetTaskInterface()->AddTask([refHandle, baseObjectSwap]() {
                    if (auto ref = refHandle.get().get()) {
                        ToggleRefCollision(ref, true);
                        ref->SetCollision(false);
                        if (baseObjectSwap) {
                            ReplaceBaseObject(ref, baseObjectSwap);
                        } else {
                            if (auto* ref3D = ref->Get3D()) {
                                ref3D->CullGeometry(true);
                            }
                        }
                    }
                });
            },
            5000);
    }
}

void ReferenceUpdater::MeltObject(RE::TESObjectREFR* ref, RE::TESBoundObject* baseObjectSwap, float meltingProgress) {
    if (ref->HasCollision()) {
        ToggleRefCollision(ref, true);
        ref->SetCollision(false);
    }
    if (auto obj3d = ref->Get3D()) {
        if (meltingProgress > 0.0f) {
            obj3d->local.rotate.entry[2][2] = meltingProgress;
            if (auto fade = obj3d->AsFadeNode()) {
                fade->GetRuntimeData().currentFade = meltingProgress;
            }
            RE::NiUpdateData updData;
            updData.flags = RE::NiUpdateData::Flag::kNone;
            updData.time = 0.0f;
            obj3d->UpdateTransformAndBounds(updData);
        } else {
            if (baseObjectSwap) {
                ReplaceBaseObject(ref, baseObjectSwap);
            } else {
                obj3d->CullGeometry(true);
            }
        }
    }
}

void ReferenceUpdater::FadeOutObject(RE::TESObjectREFR* ref, RE::TESBoundObject* baseObjectSwap,
                                     float fadeOutProgress) {
    if (auto obj3d = ref->Get3D()) {
        if (fadeOutProgress > 0.0f) {
            if (auto fade = obj3d->AsFadeNode()) {
                fade->GetRuntimeData().currentFade = fadeOutProgress;
            }
        } else {
            if (baseObjectSwap) {
                ReplaceBaseObject(ref, baseObjectSwap);
            } else {
                obj3d->CullGeometry(true);
            }
        }
    }
}

void ReferenceUpdater::RemoveFX(RE::TESObjectREFR* ref) {
    if (!ref) return;
    ref->ApplyEffectShader(EmptyFXS);
}

void ReferenceUpdater::RemoveLeavesSmooth(RE::TESObjectREFR* ref) {
    if (!ref) return;

    auto NewModel = Utils::GetModelPath(ref->GetBaseObject());

    auto DummyHazardTospawn = DummyHazards[DummyHazardIterator % DummyHazards.size()];
    DummyHazardTospawn->model = NewModel;
    DummyHazardIterator++;

    // Schedule the removal of leaves on the main thread after a delay
    clib_utilsQTR::Tasker::GetSingleton()->PushTask(
        [this, refHandle = ref->GetHandle()]() {
            SKSE::GetTaskInterface()->AddTask([this, refHandle]() {
                if (auto ref = refHandle.get().get()) {
                    RemoveLeaves(ref);
                }
            });
        },
        500);

    auto placedDummyRef = ref->PlaceObjectAtMe(DummyHazardTospawn, false);
    if (!placedDummyRef) {
        logger::error("[Main][ReferenceUpdater] RemoveLeavesSmooth: Failed to spawn Dummy ref");
        return;
    }
    placedDummyRef->SetScale(ref->GetScale());
    placedDummyRef->SetAngle(ref->GetAngle());
    RemoveCollision(placedDummyRef.get());
    RemoveStatic(placedDummyRef.get());
    // ApplyDesintegrateFX(placedDummyRef->GetHandle());
    TransformingData data;
    data.refHandle = placedDummyRef->GetHandle();
    FadingObjects.push_back(data);
}

void ReferenceUpdater::RemoveCollision(RE::TESObjectREFR* ref) {
    if (ref->HasCollision()) {
        ToggleRefCollision(ref, true);
        ref->SetCollision(false);
    }
}

void ReferenceUpdater::RemoveLeaves(RE::TESObjectREFR* ref) {
    if (!ref) return;
    if (auto obj3d = ref->Get3D()) {
        CullAlphaGeometryRecursive(obj3d);
    }
}

void ReferenceUpdater::RemoveStatic(RE::TESObjectREFR* ref) {
    if (!ref) return;
    if (auto obj3d = ref->Get3D()) {
        CullNonAlphaGeometryRecursive(obj3d);
    }
}

void ReferenceUpdater::SpawnSmokeAndDecal(RE::TESObjectREFR* ref) {
    if (!ref) return;
    static auto* WildfireMgr = WildfireMgr::GetSingleton();
    WildfireMgr->ReferenceBurned(ref->GetPosition());
}