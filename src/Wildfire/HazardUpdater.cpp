#include "Wildfire/HazardUpdater.h"
#include "Settings.h"

#include "CLibUtilsQTR/DrawDebug.hpp"
#include "CLibUtilsQTR/Tasker.hpp"

HazardUpdater::HazardUpdater() : running_(false) {
    auto tempVeryBig = RE::TESForm::LookupByEditorID("Wildfire_LargeHazard");
    auto tempBig = RE::TESForm::LookupByEditorID("Wildfire_BigHazard");
    auto tempMedium = RE::TESForm::LookupByEditorID("Wildfire_MediumHazard");
    auto tempSmall = RE::TESForm::LookupByEditorID("Wildfire_SmallHazard");

    if (tempVeryBig && tempBig && tempMedium && tempSmall) {
        Wildfire_LargeHazard = tempVeryBig->As<RE::BGSHazard>();
        Wildfire_BigHazard = tempBig->As<RE::BGSHazard>();
        Wildfire_MediumHazard = tempMedium->As<RE::BGSHazard>();
        Wildfire_SmallHazard = tempSmall->As<RE::BGSHazard>();

        auto* set = Settings::GetSingleton();
        SetHazardsPCOnly(set->HazardAffectPCOnly);
        SetHazardsLifetine(set->HazardPeriodicUpdateTime*1.1f);

        logger::info("Hazard forms found and loaded.");
    } else {
        logger::error("Hazard forms not found. Please make sure Wildfire.esp is active");
        RE::DebugMessageBox(
            "Wildfire Error: Hazard forms not found. Please make sure Wildfire.esp is active");
    }
}

void HazardUpdater::OnGameLoad() {
    ClearResult();
    {
        std::lock_guard lock(mutex_);
        lastResult_.clear();
    }
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
    lastResult_ = *result_;
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

        RE::NiPoint3 dir;

        if (maxDim == dx)
            dir = bounds[1] - bounds[0];
        else if (maxDim == dy)
            dir = bounds[3] - bounds[0];
        else
            dir = bounds[4] - bounds[0];

        float len = dir.Length();
        if (len > 0.0f) dir *= (1.0f / len);

        // bottom of trunk
        RE::NiPoint3 bottom = center - dir * (maxDim * 0.5f);

        HazardType hazardType = HazardType::SmallFireHazard;

        if (maxDim * burnProgress > 800.0f)
            hazardType = HazardType::LargeFireHazard;
        else if (maxDim * burnProgress > 400.0f)
            hazardType = HazardType::BigFireHazard;
        else if (maxDim * burnProgress > 150.0f)
            hazardType = HazardType::MediumFireHazard;

        // bottom -> 75% height
        RE::NiPoint3 pos = bottom + dir * (maxDim * 0.75f * burnProgress);
        spawnHazards.push_back({pos, hazardType});

    }


    int minX, minY, width, height;
    auto burnGrid2D = BuildBurnGrid2D(cellStateMap, minX, minY, width, height);
    if (burnGrid2D.empty()) {
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
            if (a && b && c  && d  && e && f  && g  && h  && i ) {
                // found 3x3 fire block
                float wx = minX + (x * float(VERTS_STEP));
                float wy = minY + (y * float(VERTS_STEP));

                RE::NiPoint3 pos;
                pos.x = wx;
                pos.y = wy;
                pos.z = 0.0f;  // later adjust to terrain height

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
    auto* set = Settings::GetSingleton();
    if (spawnHazards.size() >= set->MaxHazardSpawn) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        logger::debug("Found {} hazard positions in {} ms", spawnHazards.size(),
                      elapsed.count());
        logger::debug("Exiting early due to excced MaxHazardSpawn");
        return spawnHazards;
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
            if ((a && b  && c  && d ) || (a && b  && f  && i ) ||
                (a  && f  && h  && e ) || (a  && e  && g  && c )) {
                // found 2x2 fire block

                float offsetX = 0.0f;
                float offsetY = 0.0f;

                if (a && b  && c  && d ) {
                    offsetX = VERTS_STEP / 2.0f;
                    offsetY = VERTS_STEP / 2.0f;
                } else if (a && b  && f  && i ) {
                    offsetX = VERTS_STEP / 2.0f;
                    offsetY = -VERTS_STEP / 2.0f;
                } else if (a  && f  && h  && e ) {
                    offsetX = -VERTS_STEP / 2.0f;
                    offsetY = -VERTS_STEP / 2.0f;
                } else if (a  && e  && g  && c ) {
                    offsetX = -VERTS_STEP / 2.0f;
                    offsetY = VERTS_STEP / 2.0f;
                }

                float wx = minX + (x * VERTS_STEP) + offsetX;
                float wy = minY + (y * VERTS_STEP) + offsetY;

                RE::NiPoint3 pos;
                pos.x = wx;
                pos.y = wy;
                pos.z = 0.0f;  // later adjust to terrain height

                spawnHazards.push_back(SpawnHazardData{pos, HazardType::MediumFireHazard});

                // Mark all 4 cells as processed (set to 0)
                burnGrid2D[y][x] = false;
                if (a && b && c && d) {
                    burnGrid2D[y][x + 1] = false;
                    burnGrid2D[y + 1][x] = false;
                    burnGrid2D[y + 1][x + 1] = false;
                } else if (a && b && f && i ) {
                    burnGrid2D[y][x + 1] = false;
                    burnGrid2D[y - 1][x] = false;
                    burnGrid2D[y - 1][x + 1] = false;
                } else if (a  && f  && e  && h ) {
                    burnGrid2D[y - 1][x] = false;
                    burnGrid2D[y][x - 1] = false;
                    burnGrid2D[y - 1][x - 1] = false;
                } else if (a  && e  && g  && c ) {
                    burnGrid2D[y][x - 1] = false;
                    burnGrid2D[y + 1][x] = false;
                    burnGrid2D[y + 1][x - 1] = false;
                }
            } else {
                // single cell fire
                float wx = minX + (x * float(VERTS_STEP));
                float wy = minY + (y * float(VERTS_STEP));

                RE::NiPoint3 pos{wx, wy, 0};

                spawnHazards.push_back(SpawnHazardData{pos, HazardType::SmallFireHazard});

                // Mark cell as processed
                burnGrid2D[y][x] = false;
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    logger::debug("Scanned burn grid and found {} hazard positions in {} ms", spawnHazards.size(), elapsed.count());

    return spawnHazards;
}

void HazardUpdater::SpawnHazards() {
    auto start = std::chrono::high_resolution_clock::now();

    static auto* set = Settings::GetSingleton();
    static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();

    ResultType hazardsCopy;

    {
        std::lock_guard lock(mutex_);
        hazardsCopy = lastResult_;
    }

    if (hazardsCopy.empty()) {
        return;
    }

    int maxSpawn = set->MaxHazardSpawn;

    std::vector<SpawnHazardData> large_hazards, big_hazards, medium_hazards, small_hazards;
    for (const auto& data : hazardsCopy) {
        auto h = data;

        if (h.size == HazardType::SmallFireHazard)
            small_hazards.push_back(h);
        else if (h.size == HazardType::MediumFireHazard)
            medium_hazards.push_back(h);
        else if (h.size == HazardType::BigFireHazard)
            big_hazards.push_back(h);
        else 
            large_hazards.push_back(h);

        if (set->DebugMode && set->DrawHazards) {

            RE::NiColorA colour{1, 0, 0, 1};

            if (h.position.z == 0.0f) {
                float z = 0.0f;
                RE::TES::GetSingleton()->GetLandHeight(h.position, z);  // Get the terrain height at the position
                h.position.z = z;
            }
            auto p0 = h.position;
            auto p1 = p0;
            p1.z += h.size * 10;

            DebugAPI->DrawLineForMS(p0, p1, set->DebugDrawDuration, colour, h.size);
        }
    }
    
    // Shuffle each group
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::shuffle(large_hazards.begin(), large_hazards.end(), rng);
    std::shuffle(big_hazards.begin(), big_hazards.end(), rng);
    std::shuffle(medium_hazards.begin(), medium_hazards.end(), rng);
    std::shuffle(small_hazards.begin(), small_hazards.end(), rng);


    // Spawn up to maxSpawn
    int spawned = 0;
    for (const auto& sp : large_hazards) {
        if (spawned >= maxSpawn) {
            break;
        }
        SpawnHazard(sp.position, sp.size, false);
        spawned++;
    }
    for (const auto& sp : big_hazards) {
        if (spawned >= maxSpawn) {
            break;
        }
        auto size = sp.size;
        // for grass spawn Large if a lot of
        if (big_hazards.size() > maxSpawn * 10 ) {
            size = HazardType::LargeFireHazard;
        }
        SpawnHazard(sp.position, size);
        spawned++;
    }
    for (const auto& sp : medium_hazards) {
        if (spawned >= maxSpawn) {
            break;
        }
        SpawnHazard(sp.position, sp.size);
        spawned++;
    }
    for (const auto& sp : small_hazards) {
        if (spawned >= maxSpawn) {
            break;
        }
        SpawnHazard(sp.position, sp.size);
        spawned++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    logger::debug("Spawned {}/{} hazards in {} ms", spawned, hazardsCopy.size(), elapsed.count());
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


void HazardUpdater::SetHazardsLifetine(float lifetime) {
    Wildfire_LargeHazard->data.lifetime = lifetime;
    Wildfire_BigHazard->data.lifetime = lifetime;
    Wildfire_MediumHazard->data.lifetime = lifetime;
    Wildfire_SmallHazard->data.lifetime = lifetime;
}

static float RandomFloat(float min, float max) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

void HazardUpdater::SpawnHazard(const RE::NiPoint3& pos, HazardType size, bool jitter) {
    static auto* player = RE::PlayerCharacter::GetSingleton();
    static auto* tes = RE::TES::GetSingleton();

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
            logger::error("Invalid hazard size: {}", size);
            return;
    }
    if (!hazardToSpawn) {
        logger::error("Hazard form not found for size: {}", size);
        return;
    }

    auto* set = Settings::GetSingleton();

    // Set lifetime slightly longer than the update time to ensure it persists until the next update
    hazardToSpawn->data.lifetime = set->HazardPeriodicUpdateTime * 1.1f;

    RE::NiPoint3 spawnPos = pos;

    if (jitter) {
        float offsetRange = 25.0f * static_cast<float>(size);

        spawnPos.x += RandomFloat(-offsetRange, offsetRange);  // Add some random offset to the position
        spawnPos.y += RandomFloat(-offsetRange, offsetRange);
    }
    if (spawnPos.z == 0.0f) {
        float z = 0.0f;
        tes->GetLandHeight(spawnPos, z);  // Get the terrain height at the position
        spawnPos.z = z + 20.0f;
    }

    auto cell = tes->GetCell(spawnPos);
    if (cell) {
        auto waterHeight = tes->GetWaterHeight(spawnPos, cell);
        if (spawnPos.z < waterHeight) {
            spawnPos.z = waterHeight + 20.0f;
        }
    }

    auto hazardRef = player->PlaceObjectAtMe(hazardToSpawn, false);
    if (!hazardRef) {
        logger::error("Failed to place hazard");
        return;
    }
    hazardRef->SetPosition(spawnPos);

    // Random rotation around Z-axis (yaw) to add visual variety
    float yaw = RandomFloat(0.0f, 360.0f);
    hazardRef->data.angle = RE::NiPoint3{yaw, 0.0f, 0.0f};
    
    hazardRef->GetReferenceRuntimeData().refScale = static_cast<std::uint16_t>(scale * 100.0f);
}