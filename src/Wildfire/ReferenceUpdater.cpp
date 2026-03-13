#include "Wildfire/ReferenceUpdater.h"
#include "Wildfire/Wildfire.h"

#include "CLibUtilsQTR/Tasker.hpp"
// To be removed in release builds
#include "CLibUtilsQTR/DrawDebug.hpp"

#include "Settings.h"
#include "Utils.h"

#include <chrono>
#include <algorithm>
#include <random>

ReferenceUpdater::ReferenceUpdater() : running_(false) {
    auto* charredEffectShaderForm = RE::TESForm::LookupByEditorID("Wildfire_CharredFXS");
    auto* burningEffectShaderForm = RE::TESForm::LookupByEditorID("Wildfire_BurningFXS");
    auto* frostEffectShaderForm = RE::TESForm::LookupByEditorID("Wildfire_FrostFXS");
    auto* emptyEffectShaderForm = RE::TESForm::LookupByEditorID("Wildfire_EmptyFXS");
    
    
    if (charredEffectShaderForm && burningEffectShaderForm && frostEffectShaderForm && emptyEffectShaderForm) {
        Wildfire_CharredFXS = charredEffectShaderForm->As<RE::TESEffectShader>();
        Wildfire_BurningFXS = burningEffectShaderForm->As<RE::TESEffectShader>();
        Wildfire_FrostFXS = frostEffectShaderForm->As<RE::TESEffectShader>();
        Wildfire_EmptyFXS = emptyEffectShaderForm->As<RE::TESEffectShader>();
        logger::info("Effect shaders forms found and loaded.");
    } else {
        logger::error("Effect shaders forms not found. Please make sure Wildfire.esp is active");
        RE::DebugMessageBox("Wildfire Error: Effect shaders forms not found. Please make sure Wildfire.esp is active");
    }
}

static void SendFireEventsAcrossBounds(RefState refState, float wheatherFactor) {
    static auto TaskInterface = SKSE::GetTaskInterface();

    auto bounds = refState.BBox;

    float dx = (bounds[1] - bounds[0]).Length();
    float dy = (bounds[3] - bounds[0]).Length();
    float dz = (bounds[4] - bounds[0]).Length();

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

    float damage = refState.heat * 0.5f * wheatherFactor;
    auto refPos = refState.position;
    for (int i = 0; i <= steps; ++i) {
        RE::NiPoint3 pos = start + dir * (i * radius);
        TaskInterface->AddTask([pos, radius, damage, refPos]() {
            WildfireMgr::GetSingleton()->AddGrassEvent(pos, radius, damage, ElementType::Fire);

            RE::TES::GetSingleton()->ForEachReferenceInRange(
                pos, radius, [damage, pos, radius, refPos](RE::TESObjectREFR* ref) {
                    static auto* WildfireMgr = WildfireMgr::GetSingleton();
                    if (ref) {
                        if (Utils::IsSupportedForm(ref) && ref->GetPosition() != refPos) {
                            auto newRefPos = ref->GetPosition();
                            auto distance = pos.GetDistance(newRefPos);
                            float adjusted_damage = damage * (distance / radius);
                            WildfireMgr->AddRefEvent(ref, adjusted_damage, ElementType::Fire);
                            WildfireMgr->SendHitEvent(ref);
                        }
                    }

                    return RE::BSContainer::ForEachResult::kContinue;
                });
        });
    }
}

static void DrawObjectBoundsBox(RE::TESObjectREFR* ref, int lifetimeMS, const RE::NiColorA& color, float thickness) {
    if (!ref) return;

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
}

void ReferenceUpdater::OnGameLoad() {
    ClearResult();
    std::queue<RE::ObjectRefHandle> empty;
    {
        std::lock_guard lock(createRefStateMutex);
        createRefStateQueue.swap(empty);
    }
}

bool ReferenceUpdater::StartUpdate(RefEventType& refEvents, RefStateMapType& burningRefs, WeatherData& wheatherData,
                                   float delta) {

    std::queue<RE::ObjectRefHandle> objToRemove;
    static auto* set = Settings::GetSingleton();
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
            static auto* tes = RE::TES::GetSingleton();
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

                constexpr float burnThreshold = 0.30f;

                if (ratio < burnThreshold) {
                    state.isUnderwater = true;
                } else {
                    state.isUnderwater = false;
                }
            }
        }
        if (set->DebugMode && set->DrawBoundBoxes) {
            RE::NiColorA colour{0, 1, 0, 1}; // Green

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

RefStateMapType ReferenceUpdater::ReferenceUpdateLogic(RefEventType refEvents, RefStateMapType burningRefs,
                                                       WeatherData wheatherData, float delta) {
    auto start = std::chrono::high_resolution_clock::now();
    auto* set = Settings::GetSingleton();
    logger::info("Starting refs update with {} refEvents, {} refs in burningRefs and {} delta", refEvents.size(),
                 burningRefs.size(), delta);

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
                    refState->heat = std::min(newHeat, set->DefaultMaxRefHeat);
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

    auto afterEvents = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = afterEvents - start;
    logger::info("Processed {} fire refEvents in {} ms", refEvents.size(), elapsed.count());

    // Step 2 : Simulate fire and cooling for each ref in the burningRefs
    float RainingFactor = wheatherData.isRaining ? set->RainingFactor : 1.0f;
    float SnowingFactor = wheatherData.isSnowing ? set->SnowingFactor : 1.0f;
    float wheatherFactor = RainingFactor * SnowingFactor;
    for (auto& [refHandle, state] : burningRefs) {
        if (state.isCharred) {
            continue;
        } else if (state.isBurning) {
            if (state.isUnderwater) {
                state.isBurning = false;
                continue;
            }

            state.heat =
                std::min(state.heat + (set->FuelConsumptionRate * set->FuelToHeatRate * delta), set->DefaultMaxRefHeat);
            state.fuel -= set->FuelConsumptionRate * delta;

            if (state.fuel < state.initialFuel * 0.5f && state.halfEvent == false) {
                state.halfEvent = true;
                SendFireEventsAcrossBounds(state, wheatherFactor);
                state.heat *= 0.5;
            }
            if (state.fuel < state.initialFuel * 0.5f) {
                float progress = state.fuel / std::max(state.initialFuel, 0.000001f);
                TaskInterface->AddTask([this, refHandle, progress]() {
                    if (auto ref = refHandle.get().get()) {
                        RemoveLeavesSmooth(ref, progress);
                    }
                });
            }

            if (state.fuel <= 0.0f) {
                state.isBurning = false;
                state.isCharred = true;
                SendFireEventsAcrossBounds(state, wheatherFactor);
                TaskInterface->AddTask([this, refHandle]() {
                    if (auto ref = refHandle.get().get()) {
                        ApplyCharredFX(ref);
                    }
                    });
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
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    logger::debug("ReferenceUpdater: processed in {} ms", elapsed.count());
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

void ReferenceUpdater::ApplyBurningFX(RE::TESObjectREFR* ref) {
    if (!ref) return;
    ref->ApplyEffectShader(Wildfire_BurningFXS);
}

void ReferenceUpdater::ApplyCharredFX(RE::TESObjectREFR* ref) {
    if (!ref) return;
    ref->ApplyEffectShader(Wildfire_CharredFXS);
}

void ReferenceUpdater::ApplyFrostFX(RE::TESObjectREFR* ref) {
    if (!ref) return;
    ref->ApplyEffectShader(Wildfire_FrostFXS);
}

void ReferenceUpdater::RemoveFX(RE::TESObjectREFR* ref) {
    if (!ref) return;
    ref->ApplyEffectShader(Wildfire_EmptyFXS, 0.01f);
}

static void CullAlphaGeometryRecursive(RE::NiAVObject* obj) {
    if (!obj) return;

    // logger::debug("Parent {}: CullAlphaGeometryRecursive: {}", obj->parent->name.c_str(), obj->name.c_str());
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

void ReferenceUpdater::RemoveLeavesSmooth(RE::TESObjectREFR* ref, float progress) {
    if (!ref) return;

    if (auto obj3d = ref->Get3D()) {
        CullAlphaGeometryRecursive(obj3d);
    }
}


void ReferenceUpdater::RemoveLeaves(RE::TESObjectREFR* ref) {
    if (!ref) return;

    // ToDo: Make it smooth
    if (auto obj3d = ref->Get3D()) {
        CullAlphaGeometryRecursive(obj3d);
    }
}