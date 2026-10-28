#include "Wildfire/HazardUpdater.h"
#include "Settings.h"
#include "Utils.h"

#ifndef NDEBUG
#include "CLibUtilsQTR/DrawDebug.hpp"
#endif

#include "CLibUtilsQTR/Tasker.hpp"

HazardUpdater::HazardUpdater() : running_(false) {
    Wildfire_LargeHazard = nullptr;
    Wildfire_BigHazard = nullptr;
    Wildfire_MediumHazard = nullptr;
    Wildfire_SmallHazard = nullptr;

    Wildfire_LargeHazard = RE::TESForm::LookupByEditorID<RE::BGSHazard>("Wildfire_LargeHazard");
    Wildfire_BigHazard = RE::TESForm::LookupByEditorID<RE::BGSHazard>("Wildfire_BigHazard");
    Wildfire_MediumHazard = RE::TESForm::LookupByEditorID<RE::BGSHazard>("Wildfire_MediumHazard");
    Wildfire_SmallHazard = RE::TESForm::LookupByEditorID<RE::BGSHazard>("Wildfire_SmallHazard");

    auto* set = Settings::GetSingleton();
    if (Wildfire_LargeHazard && Wildfire_BigHazard && Wildfire_MediumHazard && Wildfire_SmallHazard) {
        SetHazardsPCOnly(set->HazardAffectPCOnly);
        SetHazardsLifetine(set->HazardPeriodicUpdateTime*1.1f);
        logger::info("[HazardUpdater] Fire Hazard forms found and loaded.");
    } else {
        set->ModActive = false;
        logger::error("[HazardUpdater] Fire Hazard forms not found. Please make sure Wildfire.esp is active");
        RE::DebugMessageBox(
            "Wildfire Error: Fire Hazard forms not found. Please make sure Wildfire.esp is active");
    }

    auto SmokeEditorIDs = {
        "Wildfire_SmokeLarge1x1Hazard", 
        "Wildfire_SmokeLarge2x1Hazard", 
        "Wildfire_SmokeLarge2x1VolHazard",
        "Wildfire_SmokeLow02Hazard",
        "Wildfire_SmokeLowHazard",
        //"Wildfire_SmokeLowLongHazard",
        //"Wildfire_SmokeWisps1x1Hazard",
        //"Wildfire_SmokeWisps2x1Hazard",
        //"Wildfire_SmokeWispsAdjust1x1Hazard",
        //"Wildfire_SmokeWispsAdjust2x1Hazard",
        //"Wildfire_SmokeWispsAdjustVolAdjustHazard",
        //"Wildfire_SmokeWispsAdjustVolHazard"
    };


    for (auto editorID : SmokeEditorIDs) {
        auto SmokeHazard = RE::TESForm::LookupByEditorID<RE::BGSHazard>(editorID);
        if (SmokeHazard) {
            Wildfire_SmokeHazards.push_back(SmokeHazard);
        } else {
            set->ModActive = false;
            logger::error("[HazardUpdater] Smoke Hazard forms not found. Please make sure Wildfire.esp is active");
            RE::DebugMessageBox("Wildfire Error: Smoke Hazard forms not found. Please make sure Wildfire.esp is active");
            return;
        }
    }
    logger::info("[HazardUpdater] Smoke Hazard forms found and loaded.");
}

void HazardUpdater::OnGameLoad() {
    ClearResult();
    {
        {
            std::lock_guard lock(mutex_);
            lastResult_.clear();
        }

        /*
        {
            std::lock_guard lock(FireHazardsHandlesMutex);
            ActiveLargeHazardsHandles.clear();
            ActiveBigHazardsHandles.clear();
            ActiveMediumHazardsHandles.clear();
            ActiveSmallHazardsHandles.clear();
        }

        {
            std::lock_guard lock(SmokeHazardsHandlesMutex);
            ActiveSmokeHazardsHandles.clear();
        }
        */
    }
}

bool HazardUpdater::HazardLoaded(RE::TESObjectREFR* ref) {
    if (!ref) {
        return false;
    }
    auto base = ref->GetBaseObject();
    if (base == Wildfire_LargeHazard) {
        std::lock_guard lock(FireHazardsHandlesMutex);
        ActiveLargeHazardsHandles.push_back(ref->GetHandle());
        return true;
    } else if (base == Wildfire_BigHazard) {
        std::lock_guard lock(FireHazardsHandlesMutex);
        ActiveBigHazardsHandles.push_back(ref->GetHandle());
        return true;
    } else if (base == Wildfire_MediumHazard) {
        std::lock_guard lock(FireHazardsHandlesMutex);
        ActiveMediumHazardsHandles.push_back(ref->GetHandle());
        return true;
    } else if (base == Wildfire_SmallHazard) {
        std::lock_guard lock(FireHazardsHandlesMutex);
        ActiveSmallHazardsHandles.push_back(ref->GetHandle());
        return true;
    }

    for (auto SmokeBase : Wildfire_SmokeHazards) {
        if (base == SmokeBase) {
            std::lock_guard lock(SmokeHazardsHandlesMutex);
            ActiveSmokeHazardsHandles.push_back(ref->GetHandle());
            return true;
        }
    }
    return false;
}

bool HazardUpdater::StartUpdate(const CellStateMapType& cellStateMap,
                                const RefStateMapType& burningRefs) {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return false; // Already running
    }

    clib_utilsQTR::Tasker::GetSingleton()->PushTask(
        [this, cellStateMap, burningRefs] {
            ResultType result = HazardUpdateLogic(cellStateMap, burningRefs);
            {
                std::lock_guard lock(mutex_);
                result_ = result;
            }
            running_.store(false);
        },
        0);
    return true;
}

bool HazardUpdater::IsRunning() const { return running_.load(); }

std::optional<HazardUpdater::ResultType> HazardUpdater::GetResult() {
    std::lock_guard lock(mutex_);
    return result_;
}

void HazardUpdater::CopyResult() {
    std::lock_guard lock(mutex_);
    if (result_.has_value()) {
        lastResult_ = *result_;
    }
}

void HazardUpdater::ClearResult() {
    std::lock_guard lock(mutex_);
    result_.reset();
}

static std::vector<std::vector<bool>> BuildBurnGrid2D(
    const std::unordered_map<RE::TESObjectCELL*, CellState>& cellStateMap, int& minX, int& minY, int& width,
    int& height) {

    // Calculate Boundary Box for Burning vertex
    minX = INT_MAX;
    minY = INT_MAX;
    int maxX = INT_MIN;
    int maxY = INT_MIN;

    struct GridCoord {
        int x, y;
    };
    std::vector<GridCoord> burningCoords;

    for (const auto& [cell, state] : cellStateMap) {
        const auto& cellCoords = state.cellAsCoordinates;
        int cellBaseX = cellCoords.cellX * CELL_SIZE;
        int cellBaseY = cellCoords.cellY * CELL_SIZE;

        for (int q = 0; q < CELL_QUAD_COUNT; ++q) {
            int quadOffsetX = (q % QUADS_PER_ROW) * QUAD_SIZE;
            int quadOffsetY = (q / QUADS_PER_ROW) * QUAD_SIZE;

            for (int v = 0; v < VERTS_PER_QUAD; ++v) {
                if (state.isBurning[q][v] && !state.isCharred[q][v]) {
                    int vertX = v % VERTS_PER_QUAD_ROW;
                    int vertY = v / VERTS_PER_QUAD_ROW;

                    int worldX = cellBaseX + quadOffsetX + vertX * VERTS_STEP;
                    int worldY = cellBaseY + quadOffsetY + vertY * VERTS_STEP;

                    burningCoords.push_back({worldX, worldY});
                    minX = std::min(minX, worldX);
                    minY = std::min(minY, worldY);
                    maxX = std::max(maxX, worldX);
                    maxY = std::max(maxY, worldY);
                }
            }
        }
    }

    if (burningCoords.empty()) {
        minX = minY = 0;
        width = height = 0;
        return {};
    }

    // Grid size
    width = ((maxX - minX) / VERTS_STEP) + 1;
    height = ((maxY - minY) / VERTS_STEP) + 1;

    std::vector<std::vector<bool>> burnGrid2D(height, std::vector<bool>(width, false));

    // Fill the grid
    for (const auto& coord : burningCoords) {
        int x = (coord.x - minX) / VERTS_STEP;
        int y = (coord.y - minY) / VERTS_STEP;
        burnGrid2D[y][x] = true;
    }

    return burnGrid2D;
}

HazardUpdater::ResultType HazardUpdater::HazardUpdateLogic(const CellStateMapType cellStateMap,
                                                           const RefStateMapType burningRefs) {
    auto start = std::chrono::high_resolution_clock::now();
    static auto* set = Settings::GetSingleton();

    if (set->EnableTimeLogging) {
        logger::info(
            "[Thread][HazardUpdater] HazardUpdateLogic started with {} cells in cellStateMap and {} burningRefs",
            cellStateMap.size(), burningRefs.size());
    }
    ResultType spawnHazards;

    for (const auto& [ref, state] : burningRefs) {
        if (!state.isBurning || state.isCharred) {
            continue;
        }

        float burnProgress = std::clamp(1.0f - (state.fuel / std::max(0.001f, state.initialFuel)), 0.0f, 1.0f);

        auto& bounds = state.BBox;

        float dx = (bounds[1] - bounds[0]).Length();
        float dy = (bounds[3] - bounds[0]).Length();
        float dz = (bounds[4] - bounds[0]).Length();

        std::array<float, 3> dims{dx, dy, dz};
        std::sort(dims.begin(), dims.end());

        float minDim = dims[0];
        float medDim = dims[1];
        float maxDim = dims[2];

        RE::NiPoint3 center{0, 0, 0};
        for (const auto& p : bounds) center = center + p;
        center *= (1.0f / 8.0f);

        RE::NiMatrix3 refRotate;
        refRotate.EulerAnglesToAxesZXY(state.angle);
        RE::NiPoint3 dir = refRotate * RE::NiPoint3{0, 0, 1};

        float len = dir.Length();
        if (len > 0.0f) dir /= len;

        // bottom of trunk
        RE::NiPoint3 bottom = center - dir * (maxDim * 0.5f);

        HazardType hazardType = HazardType::SmallFireHazard;

        if (maxDim * burnProgress > 800.0f)
            hazardType = HazardType::LargeFireHazard;
        else if (maxDim * burnProgress > 400.0f)
            hazardType = HazardType::BigFireHazard;
        else if (maxDim * burnProgress > 150.0f)
            hazardType = HazardType::MediumFireHazard;

        // 10% bottom -> 75% height
        bool highObject = maxDim >= 1.3f * minDim;
        RE::NiPoint3 pos;
        if (highObject) {
            float t = 0.1f + 0.65f * burnProgress;
            pos = bottom + dir * (maxDim * t);
        } else {
            pos = center;
        }
        pos -= state.position;  // make it relative to ref position
        spawnHazards.push_back({pos, hazardType, ref, highObject});
    }

    int minX, minY, width, height;
    auto burnGrid2D = BuildBurnGrid2D(cellStateMap, minX, minY, width, height);

    if (set->EnableTimeLogging) {
        auto burnGrid2DEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = burnGrid2DEnd - start;
        logger::info("[Thread][HazardUpdater] Builded 2D Burn Grid size: [{},{}] in {} ms", width, height, elapsed.count());
    }

    if (burnGrid2D.empty()) {
        if (set->EnableTimeLogging) {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;
            logger::info("[Thread][HazardUpdater] Scanned burn grid and found {} hazard positions in {} ms", spawnHazards.size(),
                         elapsed.count());
        }
        return spawnHazards;
    }

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            if (burnGrid2D[y][x] == false) continue;  // No fire here

            // Get 3x3 neighborhood values, default to 0 if out of bounds
            bool a = burnGrid2D[y][x];
            bool b = false, c = false, d = false, e = false, f = false, g = false, h = false, i = false;
            if (x + 1 < width) b = burnGrid2D[y][x + 1];
            if (y + 1 < height) c = burnGrid2D[y + 1][x];
            if (x + 1 < width && y + 1 < height) d = burnGrid2D[y + 1][x + 1];
            if (x > 0) e = burnGrid2D[y][x - 1];
            if (y > 0) f = burnGrid2D[y - 1][x];
            if (x > 0 && y + 1 < height) g = burnGrid2D[y + 1][x - 1];
            if (y > 0 && x > 0) h = burnGrid2D[y - 1][x - 1];
            if (x + 1 < width && y > 0) i = burnGrid2D[y - 1][x + 1];
            /*
            g|c|d
            e|a|b
            h|f|i
            */
            // 3x3 rectangles
            if (a && b && c && d && e && f && g && h && i) {
                // found 3x3 fire block
                float wx = minX + (x * float(VERTS_STEP));
                float wy = minY + (y * float(VERTS_STEP));

                RE::NiPoint3 pos{wx, wy, FLT_MIN};  // later adjust to terrain height

                spawnHazards.push_back(SpawnHazardData{pos, HazardType::BigFireHazard});

                // Mark all 9 cells as processed (set to false)
                burnGrid2D[y][x] = false;
                if (x + 1 < width) burnGrid2D[y][x + 1] = false;
                if (y + 1 < height) burnGrid2D[y + 1][x] = false;
                if (x + 1 < width && y + 1 < height) burnGrid2D[y + 1][x + 1] = false;
                if (x > 0) burnGrid2D[y][x - 1] = false;
                if (y > 0) burnGrid2D[y - 1][x] = false;
                if (x > 0 && y + 1 < height) burnGrid2D[y + 1][x - 1] = false;
                if (x + 1 < width && y > 0) burnGrid2D[y - 1][x + 1] = false;
            }
        }
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (burnGrid2D[y][x] <= false) continue;  // No fire here

            // Get 3x3 neighborhood values, default to 0 if out of bounds
            float a = burnGrid2D[y][x];
            float b = false, c = false, d = false, e = false, f = false, g = false, h = false, i = false;
            if (x + 1 < width) b = burnGrid2D[y][x + 1];
            if (y + 1 < height) c = burnGrid2D[y + 1][x];
            if (x + 1 < width && y + 1 < height) d = burnGrid2D[y + 1][x + 1];
            if (x > 0) e = burnGrid2D[y][x - 1];
            if (y > 0) f = burnGrid2D[y - 1][x];
            if (x > 0 && y + 1 < height) g = burnGrid2D[y + 1][x - 1];
            if (y > 0 && x > 0) h = burnGrid2D[y - 1][x - 1];
            if (x + 1 < width && y > 0) i = burnGrid2D[y - 1][x + 1];
            /*
            g|c|d
            e|a|b
            h|f|i
            */

            // 2x2 rectangles
            if ((a && b && c && d) || (a && b && f && i) || (a && f && h && e) || (a && e && g && c)) {
                // found 2x2 fire block

                float offsetX = 0.0f;
                float offsetY = 0.0f;

                if (a && b && c && d) {
                    offsetX = VERTS_STEP / 2.0f;
                    offsetY = VERTS_STEP / 2.0f;
                } else if (a && b && f && i) {
                    offsetX = VERTS_STEP / 2.0f;
                    offsetY = -VERTS_STEP / 2.0f;
                } else if (a && f && h && e) {
                    offsetX = -VERTS_STEP / 2.0f;
                    offsetY = -VERTS_STEP / 2.0f;
                } else if (a && e && g && c) {
                    offsetX = -VERTS_STEP / 2.0f;
                    offsetY = VERTS_STEP / 2.0f;
                }

                float wx = minX + (x * VERTS_STEP) + offsetX;
                float wy = minY + (y * VERTS_STEP) + offsetY;

                RE::NiPoint3 pos{wx, wy, FLT_MIN};  // later adjust to terrain height

                spawnHazards.push_back(SpawnHazardData{pos, HazardType::MediumFireHazard});

                // Mark all 4 cells as processed (set to 0)
                burnGrid2D[y][x] = false;
                if (a && b && c && d) {
                    burnGrid2D[y][x + 1] = false;
                    burnGrid2D[y + 1][x] = false;
                    burnGrid2D[y + 1][x + 1] = false;
                } else if (a && b && f && i) {
                    burnGrid2D[y][x + 1] = false;
                    burnGrid2D[y - 1][x] = false;
                    burnGrid2D[y - 1][x + 1] = false;
                } else if (a && f && e && h) {
                    burnGrid2D[y - 1][x] = false;
                    burnGrid2D[y][x - 1] = false;
                    burnGrid2D[y - 1][x - 1] = false;
                } else if (a && e && g && c) {
                    burnGrid2D[y][x - 1] = false;
                    burnGrid2D[y + 1][x] = false;
                    burnGrid2D[y + 1][x - 1] = false;
                }
            } else {
                // single cell fire
                float wx = minX + (x * float(VERTS_STEP));
                float wy = minY + (y * float(VERTS_STEP));

                RE::NiPoint3 pos{wx, wy, FLT_MIN};

                spawnHazards.push_back(SpawnHazardData{pos, HazardType::SmallFireHazard});

                // Mark cell as processed
                burnGrid2D[y][x] = false;
            }
        }
    }
    if (set->EnableTimeLogging) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        logger::info("[Thread][HazardUpdater] Scanned burn grid and found {} hazard positions in {} ms", spawnHazards.size(), elapsed.count());
    }

    return spawnHazards;
}


static bool IsVisible(const RE::NiPoint3& pos, const RE::NiPoint3& camPos, const RE::NiPoint3& camForward) {
    RE::NiPoint3 dir = pos - camPos;
    dir.Unitize();
    return camForward.Dot(dir) > 0.0f;
}


 static auto cleanupActive(std::vector<RE::ObjectRefHandle>& handles) {
    std::queue<RE::Hazard*> active;

    std::vector<RE::ObjectRefHandle> kept;
    kept.reserve(handles.size());

    for (const auto& handle : handles) {
        auto* ref = handle.get().get();
        if (!ref) {
            continue;
        }

        if (auto* hazard = ref->As<RE::Hazard>()) {
            kept.push_back(handle);
            active.push(hazard);
        }
    }
    handles.swap(kept);

    return active;
 };

 void HazardUpdater::ManageSmokeHazards() {
     static auto* set = Settings::GetSingleton();
     auto start = std::chrono::high_resolution_clock::now();

         std::lock_guard lock(SmokeHazardsHandlesMutex);
       std::queue<RE::Hazard*> active_smoke = cleanupActive(ActiveSmokeHazardsHandles);


     while (!active_smoke.empty()) {

         auto* hazard = active_smoke.front();
         active_smoke.pop();
         if (!hazard) {
             continue;
         }

         float age = hazard->GetHazardRuntimeData().age;
         float ramainAge = hazard->GetHazardRuntimeData().lifetime - age;

         float value = 500.0f;
         if (age < 5.0f) {
             value = 100 * age;
         } else if (ramainAge < 5.0f) {
             auto fade = ramainAge / 5.0f;
             if (auto haz3D = hazard->Get3D()) {
                 if (auto hazFadeNode = haz3D->AsFadeNode()) {
                     hazFadeNode->GetFadeAmount() = fade;
                 }
             }
         }

         RE::NiPoint3 pos = hazard->GetPosition();
         pos.z += value;
         hazard->SetPosition(pos);
         hazard->Update3DPosition(true);
         pos.z -= value;
         hazard->SetPosition(pos);
     }

     if (set->EnableTimeLogging && ActiveSmokeHazardsHandles.size() > 0) {
         auto end = std::chrono::high_resolution_clock::now();
         std::chrono::duration<double, std::milli> elapsed = end - start;
         logger::info("[Main][HazardUpdater] Managed {} smoke Hazards in {} ms", ActiveSmokeHazardsHandles.size(),
                      elapsed.count());
     }
 }

 void HazardUpdater::ManageFireHazards() {
     static auto* set = Settings::GetSingleton();
     static auto* tes = RE::TES::GetSingleton();
     static auto* cam = RE::PlayerCamera::GetSingleton();

     auto start = std::chrono::high_resolution_clock::now();

     int maxSpawn = set->MaxHazardSpawn;
     if (maxSpawn <= 0) {
         maxSpawn = INT_MAX;
     }

     std::queue<SpawnHazardData> large_hazards, big_hazards, medium_hazards, small_hazards;
     std::queue<RE::Hazard*> active_large, active_big, active_medium, active_small;

     ResultType hazardsCopy;
     {
         std::lock_guard lock(mutex_);
         hazardsCopy = lastResult_;
     }

     if (hazardsCopy.empty()) {
         return;
     }

     auto node = cam->cameraRoot;
     RE::NiPoint3 camPos = node->world.translate;
     RE::NiPoint3 camForward = node->world.rotate * RE::NiPoint3{0, 1, 0};

     for (auto& h : hazardsCopy) {
         if (auto ref = h.refHandle.get().get()) {
             auto refPosition = Utils::GetPosition(ref);
             h.position += refPosition;
             if (h.highObject) {
                 h.position.x = Utils::RandomFloat(refPosition.x, h.position.x);
                 h.position.y = Utils::RandomFloat(refPosition.y, h.position.y);
                 h.position.z = Utils::RandomFloat(refPosition.z, h.position.z);
             }
         }

         float z;
         if (tes->GetLandHeight(h.position, z)) {  // Get the terrain height at the position
             if (h.position.z < z || h.position.z == FLT_MIN) {
                 // Don't spawn underground, move it above the terrain
                 h.position.z = z + 20.0f;
             }
         }

         auto cell = tes->GetCell(h.position);
         float waterHeight;
         if (cell && cell->GetWaterHeight(h.position, waterHeight)) {
             if (h.position.z < waterHeight) {
                 // Don't spawn underwater, move it above the water
                 h.position.z = waterHeight + 20.0f;
             }
         }

#ifndef NDEBUG
         if (set->DebugMode && set->DrawHazards) {
             static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
             RE::NiColorA colour{1, 0, 0, 1};

             auto p0 = h.position;
             auto p1 = p0;
             p1.z += h.size * 10;

             DebugAPI->DrawLineForMS(p0, p1, set->DebugDrawDuration, colour, h.size);
         }
#endif

         bool isHazViasble = IsVisible(h.position, camPos, camForward);
         if (!isHazViasble) {
             float distance = camPos.GetSquaredDistance(h.position);
             if (distance > 2500.0f) {
                 continue;
             }
         }

         if (h.size == HazardType::SmallFireHazard)
             small_hazards.push(h);
         else if (h.size == HazardType::MediumFireHazard)
             medium_hazards.push(h);
         else if (h.size == HazardType::BigFireHazard)
             big_hazards.push(h);
         else
             large_hazards.push(h);
     }

     int visableHazards = small_hazards.size() + medium_hazards.size() + big_hazards.size() + large_hazards.size();

     // Shuffle each group
     /*
     static thread_local std::mt19937 rng{std::random_device{}()};
     std::shuffle(large_hazards.begin(), large_hazards.end(), rng);
     std::shuffle(big_hazards.begin(), big_hazards.end(), rng);
     std::shuffle(medium_hazards.begin(), medium_hazards.end(), rng);
     std::shuffle(small_hazards.begin(), small_hazards.end(), rng);
     */

     int activeSize = 0;
     {
         std::lock_guard lock(FireHazardsHandlesMutex);
         active_large = cleanupActive(ActiveLargeHazardsHandles);
         active_big = cleanupActive(ActiveBigHazardsHandles);
         active_medium = cleanupActive(ActiveMediumHazardsHandles);
         active_small = cleanupActive(ActiveSmallHazardsHandles);

         activeSize = ActiveLargeHazardsHandles.size() + ActiveBigHazardsHandles.size() +
                      ActiveMediumHazardsHandles.size() + ActiveSmallHazardsHandles.size();
     }

     int spawned = 0;
     int moved = 0;
     int limit = 0;
     while (!large_hazards.empty()) {
         if (limit >= maxSpawn * 0.25f) {
             break;
         }
         const auto& sp = large_hazards.front();
         if (!active_large.empty()) {
             auto* active = active_large.front();
             active_large.pop();
             MoveHazard(sp.position, active, sp.size, false);
             moved++;
         } else {
             SpawnHazard(sp.position, sp.size, false);
             spawned++;
         }
         limit++;
         large_hazards.pop();
     }
     limit = 0;
     while (!big_hazards.empty()) {
         if (limit >= maxSpawn * 0.25f) {
             break;
         }
         const auto& sp = big_hazards.front();
         if (!active_big.empty()) {
             auto* active = active_big.front();
             active_big.pop();
             MoveHazard(sp.position, active, sp.size);
             moved++;
         } else {
             SpawnHazard(sp.position, sp.size);
             spawned++;
         }
         limit++;
         big_hazards.pop();
     }
     limit = 0;
     while (!medium_hazards.empty()) {
         if (limit >= maxSpawn * 0.25f) {
             break;
         }
         const auto& sp = medium_hazards.front();
         if (!active_medium.empty()) {
             auto* active = active_medium.front();
             active_medium.pop();
             MoveHazard(sp.position, active, sp.size);
             moved++;
         } else {
             SpawnHazard(sp.position, sp.size);
             spawned++;
         }
         limit++;
         medium_hazards.pop();
     }
     limit = 0;
     while (!small_hazards.empty()) {
         if (limit >= maxSpawn * 0.25f) {
             break;
         }
         const auto& sp = small_hazards.front();
         if (!active_small.empty()) {
             auto* active = active_small.front();
             active_small.pop();
             MoveHazard(sp.position, active, sp.size);
             moved++;
         } else {
             SpawnHazard(sp.position, sp.size);
             spawned++;
         }
         limit++;
         small_hazards.pop();
     }
 }

void HazardUpdater::ManageHazards() {
    static auto* set = Settings::GetSingleton();

    static bool resume = false;

    if (set->EnableTimeLogging) {
        if (ActiveSmokeHazardsHandles.empty() && ActiveLargeHazardsHandles.empty() && ActiveBigHazardsHandles.empty() &&
            ActiveMediumHazardsHandles.empty() && ActiveSmallHazardsHandles.empty()) {
        } else {
            logger::info("[Main][HazardUpdater] ManageHazards started");
        }
    }

    auto start = std::chrono::high_resolution_clock::now();


    // Step 1: Cleanup and manage active smoke hazards
    ManageSmokeHazards();

    // Step 2: Manage fire hazards
    ManageFireHazards();


     if (set->EnableTimeLogging) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        logger::info("[Main][HazardUpdater] ManageHazards took {} ms", elapsed.count());
    }
}

void HazardUpdater::SetHazardsPCOnly(bool PCOnly) {
    if (PCOnly) {
        Wildfire_LargeHazard->data.flags.set(RE::BGSHazardData::BGSHazardFlags::kPCOnly);
        Wildfire_BigHazard->data.flags.set(RE::BGSHazardData::BGSHazardFlags::kPCOnly);
        Wildfire_MediumHazard->data.flags.set(RE::BGSHazardData::BGSHazardFlags::kPCOnly);
        Wildfire_SmallHazard->data.flags.set(RE::BGSHazardData::BGSHazardFlags::kPCOnly);
    } else {
        Wildfire_LargeHazard->data.flags.reset(RE::BGSHazardData::BGSHazardFlags::kPCOnly);
        Wildfire_BigHazard->data.flags.reset(RE::BGSHazardData::BGSHazardFlags::kPCOnly);
        Wildfire_MediumHazard->data.flags.reset(RE::BGSHazardData::BGSHazardFlags::kPCOnly);
        Wildfire_SmallHazard->data.flags.reset(RE::BGSHazardData::BGSHazardFlags::kPCOnly);    
    }
}

void HazardUpdater::SpawnSmokeHazardAt(const RE::NiPoint3& position) {
    static auto* player = RE::PlayerCharacter::GetSingleton();
    static auto* set = Settings::GetSingleton();

    if (ActiveSmokeHazardsHandles.size() >= set->MaxSmokeHazards && set->MaxSmokeHazards > 0) {
        return;
    }

    RE::BGSHazard* hazardToSpawn = nullptr; 
    int random = rand() % Wildfire_SmokeHazards.size();
    hazardToSpawn = Wildfire_SmokeHazards[random];

    auto hazardRef = player->PlaceObjectAtMe(hazardToSpawn, false)->As<RE::Hazard>();
    if (!hazardRef) {
        logger::error("[Main][HazardUpdater] Failed to place smoke hazard");
        return;
    }
    auto spawnPosition = position;
    spawnPosition.z -= 400 + Utils::RandomFloat(-100, 100);

    hazardRef->SetPosition(spawnPosition);

    float yaw = Utils::RandomFloat(-M_PI, M_PI);
    hazardRef->SetAngle(RE::NiPoint3{0.0f, 0.0f, yaw});

    {
        std::lock_guard lock(SmokeHazardsHandlesMutex);
        ActiveSmokeHazardsHandles.push_back(hazardRef->GetHandle());
    }
}

void HazardUpdater::SetHazardsLifetine(float lifetime) {
    Wildfire_LargeHazard->data.lifetime = lifetime;
    Wildfire_BigHazard->data.lifetime = lifetime;
    Wildfire_MediumHazard->data.lifetime = lifetime;
    Wildfire_SmallHazard->data.lifetime = lifetime;
}

void HazardUpdater::SpawnHazard(const RE::NiPoint3& pos, HazardType size, bool jitter) {
    static auto* player = RE::PlayerCharacter::GetSingleton();
    static auto* set = Settings::GetSingleton();

    RE::BGSHazard* hazardToSpawn = nullptr;
    float scale = 1.0f;
    switch (size) {
        case HazardType::SmallFireHazard:
            hazardToSpawn = Wildfire_SmallHazard;
            break;
        case HazardType::MediumFireHazard:
            hazardToSpawn = Wildfire_MediumHazard;
            break;
        case HazardType::BigFireHazard:
            hazardToSpawn = Wildfire_BigHazard;
            scale = 2.0f;
            break;
        case HazardType::LargeFireHazard:
            hazardToSpawn = Wildfire_LargeHazard;
            scale = 4.0f;
            break;
        default:
            logger::error("[Main][HazardUpdater] Invalid hazard size: {}", size);
            return;
    }
    if (!hazardToSpawn) {
        logger::error("[Main][HazardUpdater] Hazard form not found for size: {}", size);
        return;
    }

    RE::NiPoint3 spawnPos = pos;

    if (jitter) {
        float offsetRange = 25.0f * static_cast<float>(size);

        spawnPos.x += Utils::RandomFloat(-offsetRange, offsetRange);  // Add some random offset to the position
        spawnPos.y += Utils::RandomFloat(-offsetRange, offsetRange);
    }

    auto hazardRef = player->PlaceObjectAtMe(hazardToSpawn, false)->As<RE::Hazard>();
    if (!hazardRef) {
        logger::error("[Main][HazardUpdater] Failed to place fire hazard");
        return;
    }
    // Random rotation around Z-axis (yaw) to add visual variety
    // float yaw = RandomFloat(-M_PI, M_PI);

    hazardRef->SetPosition(spawnPos);
    // hazardRef->SetAngle(RE::NiPoint3{0.0f, 0.0f, yaw});
    hazardRef->SetScale(scale);

    hazardRef->GetHazardRuntimeData().lifetime = set->HazardPeriodicUpdateTime * 1.1f;
    {
        std::lock_guard lock(FireHazardsHandlesMutex);
        switch (size) {
            case HazardType::SmallFireHazard:
                ActiveSmallHazardsHandles.push_back(hazardRef->GetHandle());
                break;
            case HazardType::MediumFireHazard:
                ActiveMediumHazardsHandles.push_back(hazardRef->GetHandle());
                break;
            case HazardType::BigFireHazard:
                ActiveBigHazardsHandles.push_back(hazardRef->GetHandle());
                break;
            case HazardType::LargeFireHazard:
                ActiveLargeHazardsHandles.push_back(hazardRef->GetHandle());
                break;
            default:
                logger::error("[Main][HazardUpdater] Invalid hazard size: {}", size);
                return;
        }
    }
}

void HazardUpdater::MoveHazard(const RE::NiPoint3& pos, RE::Hazard* hazardRef, HazardType size, bool jitter) {
    static auto* set = Settings::GetSingleton();
    if (!hazardRef) {
        return;
    }

    RE::NiPoint3 newPos = pos;

    if (jitter) {
        float offsetRange = 33.3f * static_cast<float>(size);

        newPos.x += Utils::RandomFloat(-offsetRange, offsetRange);  // Add some random offset to the position
        newPos.y += Utils::RandomFloat(-offsetRange, offsetRange);
    }

    
#ifndef NDEBUG
    if (set->DebugMode && set->DrawHazards && set->DrawHazardsMovement) {
        static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
        RE::NiColorA colour{1, 0, 1, 1};  // Purple
        DebugAPI->DrawLineForMS(hazardRef->GetPosition(), newPos, set->DebugDrawDuration, colour, size);
    }
#endif

    hazardRef->SetPosition(newPos);
    hazardRef->Update3DPosition(true);

    hazardRef->GetHazardRuntimeData().age = 0.0f;
}