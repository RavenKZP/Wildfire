#include "Wildfire/GrassUpdater.h" 
#include "Wildfire/Wildfire.h"
#include "Wildfire/Weather.h"

#include "Settings.h"
#include "Utils.h"

#include "CLibUtilsQTR/Tasker.hpp"


#ifndef NDEBUG
#include "CLibUtilsQTR/DrawDebug.hpp"
#endif

GrassUpdater::GrassUpdater() : running_(false) {}

void GrassUpdater::OnGameLoad() {
    ClearResult();
    std::queue<RE::TESObjectCELL*> empty;
    {
        std::lock_guard lock(createCellStateMutex);
        createCellStateQueue.swap(empty);
    }
    {
        std::lock_guard lock(cellsToUpdateMutex);
        cellsToUpdate.clear();
    }
}

void GrassUpdater::CellLoaded(RE::TESObjectCELL* cell, CellState& state) {
    static auto* set = Settings::GetSingleton();

    for (int q = 0; q < CELL_QUAD_COUNT; ++q) {
        for (int v = 0; v < VERTS_PER_QUAD; ++v) {
            uint8_t minColor = set->MinGrassColorValue;
            uint8_t colorValue = 255;

            float maxDarkenByHeat = 0.5f;
            if (state.isCharred[q][v]) {
                // max dark and lower the grass to make it shorter
                colorValue = static_cast<uint8_t>((255 * maxDarkenByHeat) - (255 * maxDarkenByHeat));
                state.newHeights[q][v] = state.originalHeights[q][v] - set->GrassHeightDecrease;

                colorValue = std::clamp(colorValue, minColor, static_cast<uint8_t>(255));
                // Ensure we don't brighten the vertex if it's already darker (e.g., from a previous fire event)
                for (int i = 0; i < RGB_CHANNELS; i++) {
                    if (state.newColors[q][v][i] > colorValue) {
                        state.newColors[q][v][i] = colorValue;
                    }
                }
                state.altered[q][v] = true;
            }
        }
    }
}

bool GrassUpdater::StartUpdate(GrassEventType& events, CellStateMapType& cellStateMap, WeatherData& wheatherData,
                               float delta) {

    static auto* set = Settings::GetSingleton();

    std::queue<RE::TESObjectCELL*> cellsToRemove;
    for (auto& cellstate : cellStateMap) {
        RE::TESObjectCELL* cell = cellstate.first;
        CellState& state = cellstate.second;
        if (!cell || !cell->IsAttached()) {
            cellsToRemove.push(cell);
            continue;  // Skip unattached cells, they will be removed from the map
        }
        if (!state.valid) {
            cellsToRemove.push(cell);
            continue;  // Skip invalid cells, they will be removed from the map
        }

        bool alteredFound = false;
        for (int q = 0; q < CELL_QUAD_COUNT; ++q) {
            for (int v = 0; v < VERTS_PER_QUAD; ++v) {
                if (state.altered[q][v]) {
                    state.altered[q][v] = false;
                    alteredFound = true;
                }

#ifndef NDEBUG
                if (set->DebugMode && set->DrawLandscape && cell == RE::PlayerCharacter::GetSingleton()->GetParentCell()) {
                    auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
                    auto* tes = RE::TES::GetSingleton();
                    FireVertex fireVert{state.cellAsCoordinates, q, v};
                    auto p0 = fireVert.worldsPos;
                    auto p1 = p0;
                    auto p2 = p0;
                    auto p3 = p0;
                    auto p4 = p0;
                    p1.x += VERTS_STEP / 2;
                    p2.y += VERTS_STEP / 2;
                    p3.x -= VERTS_STEP / 2;
                    p4.y -= VERTS_STEP / 2;
                    tes->GetLandHeight(RE::NiPoint3{p1.x, p1.y, 0.0f}, p1.z);
                    tes->GetLandHeight(RE::NiPoint3{p2.x, p2.y, 0.0f}, p2.z);
                    tes->GetLandHeight(RE::NiPoint3{p3.x, p3.y, 0.0f}, p3.z);
                    tes->GetLandHeight(RE::NiPoint3{p4.x, p4.y, 0.0f}, p4.z);

                    RE::NiColorA colour{0.75f, 0.75f, 0.75f, 1};  // Gray

                    if (state.isBurning[q][v])
                        colour = {1, 0, 0, 1};
                    else if (state.isCharred[q][v])
                        colour = {0, 0, 0, 1};
                    else if (state.isUnderwater[q][v])
                        colour = {0, 0, 1, 1};
                    else if (state.heat[q][v] > 0 && state.canBurn[q][v])
                        colour = {1, 1, 0, 1};
                    else if (state.heat[q][v] < 0 && state.canBurn[q][v])
                        colour = {0, 1, 1, 1};
                    else if (state.canBurn[q][v])
                        colour = {0, 1, 0, 1};

                    DebugAPI->DrawLineForMS(p1, p3, set->DebugDrawDuration, colour, 1);
                    DebugAPI->DrawLineForMS(p2, p4, set->DebugDrawDuration, colour, 1);
                }
#endif
            }
        }
        if (alteredFound) {
            if (set->EnableTimeLogging) {
                logger::info("[Main][GrassUpdater] Adding cell {:X} to update grass", cell->GetFormID());
            }
            std::unique_lock cellsLock(cellsToUpdateMutex);
            cellsToUpdate.insert(cellstate);
        }
    }
    while (!cellsToRemove.empty()) {
        cellStateMap.erase(cellsToRemove.front());
        cellsToRemove.pop();
    }

    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return false;  // Already running
    }

    std::queue<RE::TESObjectCELL*> createCellState;
    {
        std::unique_lock lock(createCellStateMutex);
        createCellState.swap(createCellStateQueue);
    }
    while (!createCellState.empty()) {
        auto* cell = createCellState.front();
        if (cell) {
            GetOrCreateCellState(cellStateMap, cell);
        }
        createCellState.pop();
    }

    clib_utilsQTR::Tasker::GetSingleton()->PushTask(
        [this, events, cellStateMap, wheatherData, delta] {
            CellStateMapType result = GrassUpdateLogic(events, cellStateMap, wheatherData, delta);
            {
                std::lock_guard lock(mutex_);
                result_ = result;
            }
            running_.store(false);
        },
        0);
    return true;
}

bool GrassUpdater::IsRunning() const { return running_.load(); }

std::optional<CellStateMapType> GrassUpdater::GetResult() {
    std::lock_guard lock(mutex_);
    return result_;
}

void GrassUpdater::ClearResult() {
    std::lock_guard lock(mutex_);
    result_.reset();
}

CellStateMapType GrassUpdater::GrassUpdateLogic(GrassEventType grassEvents, CellStateMapType cellStateMap,
                                               WeatherData wheatherData, float delta) {
    auto start = std::chrono::high_resolution_clock::now();
    auto* set = Settings::GetSingleton();
    if (set->EnableTimeLogging) {
        logger::info(
            "[Thread][GrassUpdater] Starting grass update with {} grassEvents, {} cells in cellStateMap and {} delta",
            grassEvents.size(), cellStateMap.size(), delta);
    }

    // Step 1 : Process each fire event and update the corresponding vertices in the cellStateMap
    for (const auto& event : grassEvents) {
        if (event.type == ElementType::Neutral) {
            continue; // Shouldn't happen but to be sure :)
        }
        auto position = event.pos;
        auto radius = event.radius;
        auto damage = event.damage;
        if (radius > (VERTS_STEP / 2)) {
            auto NearestVertexs = FindNearestVertexsInRadius(position, radius);
            for (const auto& vertex : NearestVertexs) {
                auto cell = Utils::GetCellByCoords(vertex.cell);
                if (!cell) {
                    continue;  // Skip if cell is null, likely because the vertex is outside of loaded cells
                }
                CellState* cellState = GetCellState(cellStateMap, cell);
                if (!cellState) {
                    std::unique_lock lock(createCellStateMutex);
                    createCellStateQueue.push(cell);
                    continue;
                }

                int quad = vertex.quadrant;
                int vert = vertex.vertex;

                if (cellState->isCharred[quad][vert]) {
                    continue;
                }
                float distance = vertex.worldsPos.GetDistance(position);
                if (distance > radius) {
                    continue;  // Skip if the vertex is outside of the effective radius, should be rare due to
                               // FindNearestVertexsInRadius
                }
                float adjustedDamage = damage * (1.0f - (distance / radius));  // Scale damage by distance

                cellState->heat[quad][vert] = std::max(std::min(cellState->heat[quad][vert] + adjustedDamage, set->DefaultMaxGrassHeat), set->DefaultMinGrassHeat);  // Increase or decrease heat based on damage

                if (adjustedDamage > 0.0f) {
                    cellState->altered[quad][vert] = true;
                }
                if (cellState->canBurn[quad][vert] &&
                    cellState->heat[quad][vert] >= cellState->ignitionHeat[quad][vert] &&
                    !cellState->isCharred[quad][vert]) {
                    cellState->isBurning[quad][vert] = true;
                } else if (cellState->heat[quad][vert] <= 0.0f) {
                    cellState->isBurning[quad][vert] = false;
                }
            }

        } else {
            auto vertex = FindNearestVertex(position);
            if (vertex.worldsPos.GetDistance(position) > (VERTS_STEP / 2)) {
                continue;  // Skip if the nearest vertex is outside of the effective radius
            }
            auto cell = Utils::GetCellByCoords(vertex.cell);
            if (!cell) {
                continue;  // Skip if cell is null, likely because the vertex is outside of loaded cells
            }
            CellState* cellState = GetCellState(cellStateMap, cell);
            if (!cellState) {
                std::unique_lock lock(createCellStateMutex);
                createCellStateQueue.push(cell);
                continue;
            }
            int quad = vertex.quadrant;
            int vert = vertex.vertex;
            if (cellState->isCharred[quad][vert] || cellState->isUnderwater[quad][vert]) {
                continue;
            }
            cellState->heat[quad][vert] = std::min(cellState->heat[quad][vert] + damage, set->DefaultMaxGrassHeat);

            if (damage > 0.0f) {
                cellState->altered[quad][vert] = true;
            }
            if (cellState->canBurn[quad][vert] && cellState->heat[quad][vert] >= cellState->ignitionHeat[quad][vert] &&
                !cellState->isCharred[quad][vert]) {
                cellState->isBurning[quad][vert] = true;
            }
            if (cellState->heat[quad][vert] <= 0) {
                cellState->isBurning[quad][vert] = false;
            }
        }
    }
    auto afterEvents = std::chrono::high_resolution_clock::now();
    if (set->EnableTimeLogging) {
        std::chrono::duration<double, std::milli> elapsed = afterEvents - start;
        logger::info("[Thread][GrassUpdater] Processed {} fire grassEvents in {} ms", grassEvents.size(), elapsed.count());
    }

    // Step 2 : Simulate fire spread and cooling for each vertex in the cellStateMap
    auto* TaskInterface = SKSE::GetTaskInterface();
    float RainingFactor = wheatherData.isRaining ? set->RainingFactor : 1.0f;
    float SnowingFactor = wheatherData.isSnowing ? set->SnowingFactor : 1.0f;

    for (auto& [cell, state] : cellStateMap) {
        for (int q = 0; q < CELL_QUAD_COUNT; ++q) {
            for (int v = 0; v < VERTS_PER_QUAD; ++v) {
                if (state.isCharred[q][v]) {
                    continue;
                } else if (state.isBurning[q][v]) {
                    FireVertex targetVertex{state.cellAsCoordinates, q, v};

                    RE::NiPoint3 pos = targetVertex.worldsPos;
                    float heat = state.heat[q][v] - state.usedHeat[q][v];
                    TaskInterface->AddTask([cell, pos, heat]() {
                        auto* set = Settings::GetSingleton();
                        if (cell) {
                            cell->ForEachReferenceInRange(pos, VERTS_STEP * 2.0f, [pos, heat, set](RE::TESObjectREFR* ref) {
                                if (ref) {
                                    auto* wildfireMgr = WildfireMgr::GetSingleton();
                                    if (Utils::IsSupportedForm(ref)) {
                                        auto refPos = ref->GetPosition();
                                        auto distance = pos.GetDistance(refPos);
                                        float adjusted_damage =
                                            heat * (distance / VERTS_STEP * 2.0f) * set->FireDamageMultiplayer;
                                        wildfireMgr->AddRefEvent(ref, adjusted_damage,
                                                                                 ElementType::Fire);
                                    }
                                    wildfireMgr->SendHitEvent(ref);
                                }
                                return RE::BSContainer::ForEachResult::kContinue;
                            });
                        }
                    });

                    auto neighbours = GetFireVertexNeighboursWeighted(targetVertex, wheatherData.windData);

                    for (const auto& neighbour : neighbours) {
                        float heatToUse = std::min(state.heat[q][v] - state.usedHeat[q][v], set->MaxHeatDistribution);
                        float damage = 0.0f;
                        if (heatToUse < 0.0f) {
                            heatToUse = 0.0f;
                        } else {
                            damage =
                                (((heatToUse / set->HeatDistributionFactor) * delta) * (RainingFactor * SnowingFactor));
                            state.usedHeat[q][v] += damage;
                            int nq = neighbour.vertex.quadrant;
                            int nv = neighbour.vertex.vertex;

                            if (state.cellAsCoordinates != neighbour.vertex.cell) {
                                auto ncell = Utils::GetCellByCoords(neighbour.vertex.cell);
                                if (!ncell) {
                                    continue;  // Skip if cell is null, likely because the vertex is outside of loaded
                                               // cells
                                }
                                auto neighbourCellState = GetCellState(cellStateMap, ncell);
                                if (!neighbourCellState) {
                                    std::unique_lock lock(createCellStateMutex);
                                    createCellStateQueue.push(ncell);
                                    continue;
                                }
                                neighbourCellState->heat[nq][nv] += damage * neighbour.weight;
                                neighbourCellState->altered[nq][nv] = true;
                                if (neighbourCellState->canBurn[nq][nv] &&
                                    neighbourCellState->heat[nq][nv] >= neighbourCellState->ignitionHeat[nq][nv] &&
                                    !state.isCharred[nq][nv]) {
                                    neighbourCellState->isBurning[nq][nv] = true;
                                }

                            } else {
                                state.heat[nq][nv] += damage * neighbour.weight;
                                state.altered[nq][nv] = true;
                                if (state.canBurn[nq][nv] && state.heat[nq][nv] >= state.ignitionHeat[nq][nv] &&
                                    !state.isCharred[nq][nv]) {
                                    state.isBurning[nq][nv] = true;
                                }
                            }
                        }
                    }

                    // Decrease fuel amount
                    state.fuel[q][v] -= set->FuelConsumptionRate * delta;
                    // Heat increases as fuel burns
                    state.heat[q][v] =
                        std::min(state.heat[q][v] + (set->FuelConsumptionRate * set->FuelToHeatRate * set->FireDamageMultiplayer * delta),
                                 set->DefaultMaxGrassHeat);
                    state.altered[q][v] = true;

                    // Mark as charred when burning stops
                    if (state.fuel[q][v] <= 0.0f) {
                        state.isBurning[q][v] = false;
                        state.isCharred[q][v] = true;
                        if (v % set->DecalSpacing == 0) {
                            auto pos = targetVertex.worldsPos;
                            SKSE::GetTaskInterface()->AddTask([pos]() { 
                                WildfireMgr::GetSingleton()->GrassBurned(pos); });
                        }

                    }
                } else if (state.heat[q][v] > 0.0f) {
                    // Cool down the fire cell if not burning
                    state.heat[q][v] -= set->SelfHeatLoss * delta;
                    if (state.heat[q][v] < 0.0f) {
                        state.heat[q][v] = 0.0f;
                    }
                } else if (state.heat[q][v] < 0.0f) {
                    // Heat up the fire cell if below zero
                    state.heat[q][v] += set->SelfColdLoss * delta;
                    if (state.heat[q][v] > 0.0f) {
                        state.heat[q][v] = 0.0f;
                    }
                }
            }
        }
    }
    auto afterSpread = std::chrono::high_resolution_clock::now();
    if (set->EnableTimeLogging) {
        std::chrono::duration<double, std::milli> elapsed = afterSpread - afterEvents;
        logger::info("[Thread][GrassUpdater] Simulated fire spread and cooling in {} ms", elapsed.count());
    }

    // Step 3 : Update colors for altered vertices in the cellStateMap based on their new state (burning, charred, etc.)
    // to be applied later in the main thread
    for (auto& [cell, state] : cellStateMap) {
        for (int q = 0; q < CELL_QUAD_COUNT; ++q) {
            for (int v = 0; v < VERTS_PER_QUAD; ++v) {
                if (state.altered[q][v]) {
                    if (state.heat[q][v] < 0.0f) {
                        continue;
                    }
                    uint8_t minColor = set->MinGrassColorValue;
                    uint8_t colorValue = 255;

                    float maxDarkenByHeat = 0.5f;
                    if (state.isCharred[q][v]) {
                        // max dark and lower the grass to make it shorter
                        colorValue = static_cast<uint8_t>((255 * maxDarkenByHeat) - (255 * maxDarkenByHeat));
                        state.newHeights[q][v] = state.originalHeights[q][v] - set->GrassHeightDecrease;
                    } else if (state.isBurning[q][v]) {
                        // Darken based on fuel amount (the less fuel, the darker)
                        float fuelRatio = state.fuel[q][v] / set->DefaultGrassFuel;
                        colorValue = static_cast<uint8_t>((255 * maxDarkenByHeat) -
                                                          ((255 * maxDarkenByHeat) * (1.0f - fuelRatio)));
                    } else {
                        // Darken based on heat (the more heat, the darker)
                        float heatRatio = state.heat[q][v] / state.ignitionHeat[q][v];
                        colorValue = static_cast<uint8_t>((255 * maxDarkenByHeat) +
                                                          ((255 * maxDarkenByHeat) * (1.0f - heatRatio)));
                    }

                    colorValue = std::clamp(colorValue, minColor, static_cast<uint8_t>(255));
                    // Ensure we don't brighten the vertex if it's already darker (e.g., from a previous fire event)
                    for (int i = 0; i < RGB_CHANNELS; i++) {
                        if (state.newColors[q][v][i] > colorValue) {
                            state.newColors[q][v][i] = colorValue;
                        }
                    }
                }
            }
        }
    }
    auto afterColor = std::chrono::high_resolution_clock::now();
    if (set->EnableTimeLogging) {
        std::chrono::duration<double, std::milli> elapsed = afterColor - afterSpread;
        logger::info("[Thread][GrassUpdater] Updated vertex colors and heights in {} ms", elapsed.count());
        elapsed = afterColor - start;
        logger::info("[Thread][GrassUpdater] Total grass update logic time: {} ms", elapsed.count());
    }

    return cellStateMap;
}

CellState* GrassUpdater::GetOrCreateCellState(CellStateMapType& cellStateMap,
                                              RE::TESObjectCELL* cell) {
    if (!cell) {
        return nullptr;
    }
    auto it = cellStateMap.find(cell);
    if (it != cellStateMap.end()) {
        return &it->second;  // Already exists, return it
    }
    auto [newIt, _] = cellStateMap.emplace(cell, CellState(cell));
    if (&newIt->second.valid) {
        return &newIt->second;
    } else {
        cellStateMap.erase(cell);
    }
    return nullptr;
}

CellState* GrassUpdater::GetCellState(CellStateMapType& cellStateMap, RE::TESObjectCELL* cell) {
    if (!cell) {
        return nullptr;
    }
    auto it = cellStateMap.find(cell);
    if (it != cellStateMap.end()) {
        return &it->second;  // Already exists, return it
    }
    return nullptr;
}

FireVertex GrassUpdater::FindNearestVertex(const RE::NiPoint3& pos) {
    // Local position within the cell (0 to 4096)
    int cellX = static_cast<int>(std::floor(pos.x / CELL_SIZE));
    int cellY = static_cast<int>(std::floor(pos.y / CELL_SIZE));
    float localX = pos.x - (cellX * CELL_SIZE);
    float localY = pos.y - (cellY * CELL_SIZE);

    // Determine q:
    // Quadrants are split like:
    //  0 | 1
    // ---+---
    //  2 | 3
    // Determine q
    int quadrantX = (localX >= QUAD_SIZE) ? 1 : 0;
    int quadrantY = (localY >= QUAD_SIZE) ? 1 : 0;
    int quadrant = quadrantY * QUADS_PER_ROW + quadrantX;

    // Convert to q-local coordinates
    float quadLocalX = localX - (quadrantX * QUAD_SIZE);
    float quadLocalY = localY - (quadrantY * QUAD_SIZE);

    // Vertex index in 17x17 grid
    int vertX = std::clamp(static_cast<int>(std::round(quadLocalX / VERTS_STEP)), 0, VERTS_PER_QUAD_ROW - 1);
    int vertY = std::clamp(static_cast<int>(std::round(quadLocalY / VERTS_STEP)), 0, VERTS_PER_QUAD_ROW - 1);
    int vertIndex = vertY * VERTS_PER_QUAD_ROW + vertX;

    CellAsCoords cell{cellX, cellY};
    FireVertex key{cell, quadrant, vertIndex};
    return key;
}

std::vector<FireVertex> GrassUpdater::FindNearestVertexsInRadius(const RE::NiPoint3& pos, float radius) {
    std::vector<FireVertex> result;
    FireVertex centerVertex = FindNearestVertex(pos);

    auto centerCellX = centerVertex.cell.cellX;
    auto centerCellY = centerVertex.cell.cellY;

    // Calculate bounding box in world coordinates
    float minX = pos.x - radius;
    float maxX = pos.x + radius;
    float minY = pos.y - radius;
    float maxY = pos.y + radius;

    // Convert bounding box to cell coordinates
    int minCellX = static_cast<int>(std::floor(minX / CELL_SIZE));
    int maxCellX = static_cast<int>(std::floor(maxX / CELL_SIZE));
    int minCellY = static_cast<int>(std::floor(minY / CELL_SIZE));
    int maxCellY = static_cast<int>(std::floor(maxY / CELL_SIZE));

    // Clamp to loaded cell range [-2, 2]
    int relMinCellDX = std::max(minCellX - centerCellX, -2);
    int relMaxCellDX = std::min(maxCellX - centerCellX, 2);
    int relMinCellDY = std::max(minCellY - centerCellY, -2);
    int relMaxCellDY = std::min(maxCellY - centerCellY, 2);

    for (int cellDX = relMinCellDX; cellDX <= relMaxCellDX; ++cellDX) {
        for (int cellDY = relMinCellDY; cellDY <= relMaxCellDY; ++cellDY) {
            int cellX = centerCellX + cellDX;
            int cellY = centerCellY + cellDY;
            for (int q = 0; q < CELL_QUAD_COUNT; ++q) {
                for (int v = 0; v < VERTS_PER_QUAD; ++v) {
                    FireVertex candidate{CellAsCoords{cellX, cellY}, q, v};
                    if (candidate.worldsPos.GetDistance(pos) <= radius) {
                        result.push_back(candidate);
                    }
                }
            }
        }
    }
    return result;
}

std::vector<FireVertex> GrassUpdater::GetFireVertexNeighbours(const FireVertex& vertex) {
    std::vector<FireVertex> result;


    // Convert vertex index to (x, y) in the q grid
    int x = vertex.vertex % VERTS_PER_QUAD_ROW;
    int y = vertex.vertex / VERTS_PER_QUAD_ROW;

    // Convert q to (qx, qy)
    int qx = vertex.quadrant % QUADS_PER_ROW;
    int qy = vertex.quadrant / QUADS_PER_ROW;

    auto cellX = vertex.cell.cellX;
    auto cellY = vertex.cell.cellY;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;  // Exclude center

            int nx = x + dx;
            int ny = y + dy;
            int nqx = qx;
            int nqy = qy;
            int ncellX = cellX;
            int ncellY = cellY;

            // Handle crossing q boundaries
            if (nx < 0) {
                nqx -= 1;
                nx = VERTS_PER_QUAD_ROW - 1;
            } else if (nx >= VERTS_PER_QUAD_ROW) {
                nqx += 1;
                nx = 0;
            }
            if (ny < 0) {
                nqy -= 1;
                ny = VERTS_PER_QUAD_ROW - 1;
            } else if (ny >= VERTS_PER_QUAD_ROW) {
                nqy += 1;
                ny = 0;
            }

            // Handle crossing cell boundaries
            if (nqx < 0) {
                ncellX -= 1;
                nqx = 1;
            } else if (nqx > 1) {
                ncellX += 1;
                nqx = 0;
            }
            if (nqy < 0) {
                ncellY -= 1;
                nqy = 1;
            } else if (nqy > 1) {
                ncellY += 1;
                nqy = 0;
            }

            int nquad = nqy * QUADS_PER_ROW + nqx;

            CellAsCoords ncell = vertex.cell;
            if (ncellX != cellX || ncellY != cellY) {
                ncell = CellAsCoords(ncellX, ncellY);
            }

            int nvertex = ny * VERTS_PER_QUAD_ROW + nx;
            result.push_back(FireVertex{ncell, nquad, nvertex});
        }
    }
    return result;
}

std::vector<WeightedNeighbour> GrassUpdater::GetFireVertexNeighboursWeighted(const FireVertex& vertex,
                                                                            WindData windData) {
    std::vector<WeightedNeighbour> result;

    std::vector<FireVertex> neighbours = GetFireVertexNeighbours(vertex);
    if (neighbours.size() != 8) {
        // Unexpected
        for (auto& neigh : neighbours) {
            WeightedNeighbour wn{neigh, 1.0f};
            result.push_back(wn);
        }
        return result;
    }

    // Predefined direction vectors matching neighbour ordering:
    // 0: NW, 1: N, 2: NE, 3: W, 4: E, 5: SW, 6: S, 7: SE
    const std::array<std::pair<float, float>, 8> dirVec = {
        std::make_pair(-1.0f, -1.0f),  // NW
        std::make_pair(0.0f, -1.0f),   // N
        std::make_pair(1.0f, -1.0f),   // NE
        std::make_pair(-1.0f, 0.0f),   // W
        std::make_pair(1.0f, 0.0f),    // E
        std::make_pair(-1.0f, 1.0f),   // SW
        std::make_pair(0.0f, 1.0f),    // S
        std::make_pair(1.0f, 1.0f),    // SE
    };

    // Base weights aligning with earlier: cardinal=1.0, diagonal=0.7
    const std::array<float, 8> baseWeights = {
        0.7f,  // NW
        1.0f,  // N
        0.7f,  // NE
        1.0f,  // W
        1.0f,  // E
        0.7f,  // SW
        1.0f,  // S
        0.7f,  // SE
    };

    // Wind direction vector (assumed radians)
    float windDirX = std::cosf(windData.direction);
    float windDirY = std::sinf(windData.direction);

    float windLen = std::sqrt(windDirX * windDirX + windDirY * windDirY);
    if (windLen > 0.0f) {
        windDirX /= windLen;
        windDirY /= windLen;
    }

    // For each neighbour, compute weight
    size_t count = std::min(neighbours.size(), dirVec.size());
    for (size_t i = 0; i < count; ++i) {
        const FireVertex& neigh = neighbours[i];

        // Direction unit vector for this neighbour
        float neighDirX = dirVec[i].first;
        float neighDirY = dirVec[i].second;
        float len = std::sqrt(neighDirX * neighDirX + neighDirY * neighDirY);
        if (len > 0.0f) {
            neighDirX /= len;
            neighDirY /= len;
        }

        float dot = windDirX * neighDirX + windDirY * neighDirY;
        float windBoost =
            dot * windData.speed * Settings::GetSingleton()->WindSpeedFactor;  // alignment * speed * factor
        float finalWeight = baseWeights[i] * (1.0f + windBoost);

        result.push_back(WeightedNeighbour{neigh, finalWeight});
    }

    std::sort(result.begin(), result.end(),
              [](const WeightedNeighbour& a, const WeightedNeighbour& b) { return a.weight > b.weight; });

    return result;
}

void GrassUpdater::LazyCreateCellState(CellStateMapType& cellStateMap) {
    RE::TES::GetSingleton()->ForEachCell([this, &cellStateMap](RE::TESObjectCELL* cell) {
        if (cell) {
            if (!cellStateMap.contains(cell)) {
                GetOrCreateCellState(cellStateMap, cell);
                return RE::BSContainer::ForEachResult::kStop; 
            }
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
}

bool GrassUpdater::RegenerateGrass() {
    auto start = std::chrono::high_resolution_clock::now();
    static auto* set = Settings::GetSingleton();
    bool regenerated = false;
    std::unique_lock lock(cellsToUpdateMutex);

    if (!cellsToUpdate.empty()) {  // One cell per frame
        if (set->EnableTimeLogging) {
            logger::info("[Main][GrassUpdater] Cells To Update {} ", cellsToUpdate.size());
        }
        auto it = cellsToUpdate.begin();
        RE::TESObjectCELL* cellToApply = it->first;
        CellState& state = it->second;
        if (set->EnableTimeLogging) {
            logger::info("[Main][GrassUpdater] Regenerate Grass In Cell {:X} ", cellToApply->GetFormID());
        }

        if (cellToApply) {
            auto land = cellToApply->GetLandscape();
            if (land) {
                std::memcpy(land->loadedData->colors, state.newColors, sizeof(state.newColors));
                std::memcpy(land->loadedData->heights, state.newHeights, sizeof(state.newHeights));
            }
            RegenerateGrassInCell(cellToApply);
            regenerated = true;
            if (land) {
                std::memcpy(land->loadedData->colors, state.originalColors, sizeof(state.originalColors));
                std::memcpy(land->loadedData->heights, state.originalHeights, sizeof(state.originalHeights));
            }
        }
        cellsToUpdate.erase(it);
        lock.unlock();
    }
    if (set->EnableTimeLogging && regenerated) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        logger::info("[Main][GrassUpdater] RegenerateGrass took {} ms", elapsed.count());
    }
    return regenerated;
}

// This is where the magic happens - we tell the game to regenerate the grass in the specified cell
// that way we can apply our changes to the grass colors based on the fire state
void GrassUpdater::RegenerateGrassInCell(RE::TESObjectCELL* cell) {
    if (cell && cell->IsAttached()) {
        auto grassMgr = RE::BGSGrassManager::GetSingleton();
        //DarkenGrassShader(cell);

        volatile bool flag = false;
        /* I'm stupid all it takes is to add Wildfire.ini with the following line to disable grass fade-in effect
        [Grass]
        bEnableGrassFade=0
        */
        //REL::Relocation<std::uint8_t*> GrassFadeFlag{REL::RelocationID(501130, 359446)};
        //*GrassFadeFlag = false;  // This disable grass fade-in effect, so grass pop in imidietally

        grassMgr->RemoveCellGrass(cell);
        grassMgr->AddCellGrass(cell, &flag);
        grassMgr->ExecuteAllGrassTasks(cell, 0);  // actually apply grass to scene?
                                                   // 2'nd argument is just set to 1 internally (no matter what we pass)

        //cell->GetRuntimeData().cellLand->SetAltered(true);
        
    }
}

void GrassUpdater::DarkenGrassShader(RE::TESObjectCELL* cell) {
    auto cellGrassData = cell->extraList.GetByType(RE::ExtraDataType::kCellGrassData);
    if (cellGrassData) {
        auto grassData = static_cast<RE::ExtraCellGrassData*>(cellGrassData);
        for (auto grassHandler : grassData->grassHandles) {
            if (grassHandler) {

                auto* ms = static_cast<RE::BSMultiStreamInstanceTriShape*>(grassHandler->triShape.get());

                auto& rt = ms->GetMultiStreamTrishapeRuntimeData();
                ms->SetMaterialNeedsUpdate(true);

                RE::NiUpdateData updData;
                updData.flags = RE::NiUpdateData::Flag::kNone;
                updData.time = 0.0f;
                grassHandler->triShape->UpdateTransformAndBounds(updData);

            }
        }
    }
}