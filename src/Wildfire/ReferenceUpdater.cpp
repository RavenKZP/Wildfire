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

    for (int i = 0; i <= steps; ++i) {
        RE::NiPoint3 pos = start + dir * (i * radius);
        TaskInterface->AddTask([pos, radius, damage]() {
            WildfireMgr::GetSingleton()->AddGrassEvent(pos, radius, damage, ImpactType::Fire);
        });
    }
}

static void DrawObjectBoundsBox(RE::TESObjectREFR* ref, int lifetimeMS, const RE::NiColorA& color, float thickness) {
    if (!ref) return;

    // Local bounds
    auto points = Utils::GetBoundingBox(ref);

    auto DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();

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
    std::queue<RE::TESObjectREFR*> empty;
    {
        std::lock_guard lock(createRefStateMutex);
        createRefStateQueue.swap(empty);
    }
}

bool ReferenceUpdater::StartUpdate(RefEventType& refEvents, RefStateMapType& burningRefs, WeatherData& wheatherData,
                                   float delta) {

    std::queue<RE::TESObjectREFR*> objToRemove;
    auto* set = Settings::GetSingleton();
    for (auto& [ref, state] : burningRefs) {
        if (!ref) {
            objToRemove.push(ref);
            continue;
        }
        if (set->DebugMode) {
            RE::NiColorA colour{0, 1, 0, 1}; // Green

            if (state.isBurning)
                colour = {1, 0, 0, 1};
            else if (state.isCharred)
                colour = {0, 0, 0, 1};
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

    std::queue<RE::TESObjectREFR*> createRefState;
    {
        std::unique_lock lock(createRefStateMutex);
        createRefState.swap(createRefStateQueue);
    }
    while (!createRefState.empty()) {
        auto* ref = createRefState.front();
        if (ref) {
            GetOrCreateRefState(burningRefs, ref);
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
        auto ref = event.obj;
        auto damage = event.damage;
        auto type = event.type;
        if (ref) {
            auto refState = GetRefState(burningRefs, ref);
            if (!refState) {
                std::unique_lock lock(createRefStateMutex);
                createRefStateQueue.push(ref);
                continue;
            }
            if (!refState->isCharred) {
                if (type == ImpactType::Fire || type == ImpactType::Shock) {
                    damage = std::max(0.0f, damage);  // can't cool
                }

                if (type == ImpactType::Water || type == ImpactType::Cold) {
                    damage = std::min(0.0f, damage);  // can't heat
                }

                float newHeat = refState->heat + damage;

                if (type == ImpactType::Shock && refState->heat >= refState->minBurnHeat) {
                    // already burning = ignore
                } else if (type == ImpactType::Shock) {
                    refState->heat = std::min(newHeat, refState->minBurnHeat);
                } else if (type == ImpactType::Water) {
                    refState->heat = std::max(newHeat, 0.0f);
                } else {
                    refState->heat = newHeat;
                }

                if (refState->heat >= refState->minBurnHeat && refState->canBurn && !refState->isBurning) {
                    refState->isFrozen = false;
                    refState->isBurning = true;
                    TaskInterface->AddTask([this, ref]() { ApplyBurningFX(ref); });
                }
                if (refState->heat <= -refState->minBurnHeat) {
                    refState->heat = -refState->minBurnHeat;
                    if (!refState->isFrozen) {
                        refState->isFrozen = true;
                        refState->isBurning = false;
                        TaskInterface->AddTask([this, ref]() { ApplyFrostFX(ref); });
                    }
                }
                if (refState->heat <= 0.0f && refState->isBurning) {
                    refState->isBurning = false;
                    TaskInterface->AddTask([this, ref]() { RemoveFX(ref); });
                }
                if (refState->heat >= 0.0f && refState->isFrozen) {
                    refState->isFrozen = false;
                    TaskInterface->AddTask([this, ref]() { RemoveFX(ref); });
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
    for (auto& [ref, state] : burningRefs) {
        if (state.isCharred) {
            continue;
        } else if (state.isBurning) {
            state.heat += set->FuelConsumptionRate * set->FuelToHeatRate * delta;
            state.fuel -= set->FuelConsumptionRate * delta;

            if (state.fuel < state.initialFuel / 2.0f && state.halfEvent == false) {
                state.halfEvent = true;
                SendFireEventsAcrossBounds(state, wheatherFactor);
                TaskInterface->AddTask([this, ref]() { RemoveLeaves(ref); });
            }

            if (state.fuel <= 0.0f) {
                state.isBurning = false;
                state.isCharred = true;
                TaskInterface->AddTask([this, ref]() { ApplyCharredFX(ref); });
                SendFireEventsAcrossBounds(state, wheatherFactor);
            }
        } else if (state.heat > 0.0f) {
            // Cool down the ref if not burning
            state.heat -= set->SelfHeatLoss * delta;
            if (state.heat <= 0.0f) {
                state.heat = 0.0f;
                TaskInterface->AddTask([this, ref]() { RemoveFX(ref); });
            }
        } else if (state.heat < 0.0f) {
            // Heat up the ref if below zero
            state.heat += set->SelfColdLoss * delta;
            if (state.heat >= 0.0f) {
                state.heat = 0.0f;
                state.isFrozen = false;
                TaskInterface->AddTask([this, ref]() { RemoveFX(ref); });
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    logger::debug("ReferenceUpdater: processed in {} ms", elapsed.count());
    return burningRefs;
}

RefState* ReferenceUpdater::GetOrCreateRefState(RefStateMapType& burningRefs, RE::TESObjectREFR* ref) {
    if (!ref) {
        return nullptr;
    }
    auto it = burningRefs.find(ref);
    if (it != burningRefs.end()) {
        return &it->second;  // Already exists, return it
    }
    auto [newIt, _] = burningRefs.emplace(ref, RefState(ref));
    return &newIt->second;
}

RefState* ReferenceUpdater::GetRefState(RefStateMapType& burningRefs, RE::TESObjectREFR* ref) {
    if (!ref) {
        return nullptr;
    }
    auto it = burningRefs.find(ref);
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
        bool hasAlpha = false;
        for (int i = 0; i < RE::BSGeometry::States::kTotal; ++i) {
            auto* prop = geomData.properties[i].get();
            if (prop) {
                auto type = prop->GetType();
                if (type == RE::NiProperty::Type::kAlpha) {
                    hasAlpha = true;
                    break;
                }
                /* textures replacement, not enough data yet
                * maybe wecan replace whole shader property with our own that has different texture?
                if (type == RE::NiProperty::Type::kShade) {
                    auto* shadeProp = static_cast<RE::NiShadeProperty*>(prop);
                }
                */
            }
        }
        if (hasAlpha) {
            triShape->CullGeometry(true);
        }
    }

    if (obj->AsNode()) {
        for (auto& child : obj->AsNode()->GetChildren()) {
            if (child) CullAlphaGeometryRecursive(child.get());
        }
    }
}

void ReferenceUpdater::RemoveLeaves(RE::TESObjectREFR* ref) {
    if (!ref) return;

    // ToDo: Make it smooth
    if (auto obj3d = ref->Get3D()) {
        CullAlphaGeometryRecursive(obj3d);
    }
}

/*
void ReferenceUpdater::ApplyFX(RefStateMapType& burningRefs, float delta) {
    static float accumulatedTime = 0.0f;
    const float updateInterval = 0.25f;  // how often FX threshold increases

    accumulatedTime += delta;

    if (accumulatedTime < updateInterval) {
        return;
    }
    // keep remainder so timing stays smooth
    accumulatedTime -= updateInterval;

    for (auto& [ref, state] : burningRefs) {
        if (ref) {
            if (state.isBurning) {
                RemoveLeaves(ref);
            }
        }
    }
}
*/