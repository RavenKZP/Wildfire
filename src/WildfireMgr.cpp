#include "WildfireMgr.h"
#include "HazardMgr.h"
#include "Settings.h"
#include "Utils.h"

void WildfireMgr::PeriodicUpdate(float delta) {
    auto* set = Settings::GetSingleton();
    for (auto& fireCell : fireCellMap) {
        if (fireCell.second.altered) {
            RE::BGSGrassManager* GrassMgr = RE::BGSGrassManager::GetSingleton();

            REL::Relocation<std::uint8_t*> byteFlag{REL::ID(359446)};
            *byteFlag = 0;
            GrassMgr->RemoveGrassInCell(fireCell.first);
            // Trigger new grass creation
            std::uint8_t flag = 0;
            GrassMgr->CreateGrassInCell(fireCell.first, &flag);
            GrassMgr->ExecuteAllGrassTasks(fireCell.first, flag);
            //*byteFlag = 1;

            fireCell.second.altered = false;  // Reset altered state after processing
        }
        for (int q = 0; q < 4; ++q) {
            for (int v = 0; v < 289; ++v) {
                auto& colors = fireCell.first->GetRuntimeData().cellLand->loadedData->colors[q][v];
                if (fireCell.second.isBurning[q][v]) { 

                    // ToDo: Get Wind Force and direction
                    // ToDo: Neighbours veight (0.7 for diagonal, 1.0 for cardinal directions)

                    FireVertex targetVertex{fireCell.first, q, v};

                    auto isRaining = IsCurrentWeatherRaining();
                    auto windData = GetCurrentWind();

                    auto neighbours = GetFireVertexNeighbours(targetVertex);

                    for (const auto& neighbour : neighbours) {
                        float RainingFactor = isRaining ? set->RainingFactor : 1.0f;
                        float damage =
                            (((fireCell.second.heat[q][v] / set->HeatDistributionFactor) * delta) * RainingFactor);
                        // Damage neighbouring cells
                        DamageFireCell(neighbour, damage, true);
                    }

                    // Decrease heat
                    fireCell.second.heat[q][v] -=
                        neighbours.size() * (fireCell.second.heat[q][v] / set->HeatDistributionFactor) * delta;

                    // Decrease fuel amount
                    fireCell.second.fuel[q][v] -= set->FuelConsumptionRate * delta;
                    // Heat increases as fuel burns
                    fireCell.second.heat[q][v] += set->FuelConsumptionRate * set->FuelToHeatRate * delta;

                    // Mark as charred when burning stops
                    if (fireCell.second.fuel[q][v] <= 0.0f) {
                        fireCell.second.isBurning[q][v] = false;
                        fireCell.second.isCharred[q][v] = true;

                        colors[0] = 0;  // R
                        colors[1] = 0;  // G
                        colors[2] = 0;  // B
                        // Mark the Cell as altered by fire
                        fireCell.second.altered = true;
                    } else {
                        // Update color based on fuel left
                        float fuelRatio = fireCell.second.fuel[q][v] / set->InitialFuelAmount;
                        uint8_t colorValue = static_cast<uint8_t>(128.0f - (128.0f * (1.0f - fuelRatio)));
                        if (colors[0] > colorValue || colors[1] > colorValue || colors[2] > colorValue) {
                            colors[0] = colorValue;  // R
                            colors[1] = colorValue;  // G
                            colors[2] = colorValue;  // B
                            // Mark the Cell as altered by fire
                            fireCell.second.altered = true;
                        }
                    }
                } else if (fireCell.second.heat[q][v] > 0) {  
                    // Cool down the fire cell if not burning
                    fireCell.second.heat[q][v] -= set->SelfHeatLoss * delta;
                }
            }
        }
    }
}

void WildfireMgr::AddFireEvent(const RE::NiPoint3& impactPos, float radius, float damage) {
    if (radius > 128.0f) {  // This Will affect more than one vertex
        auto NearestVertexs = FindNearestVertexsInRadius(impactPos, radius);
        for (const auto& vertex : NearestVertexs) {
            float distance = Utils::GetWorldPosition(vertex).GetDistance(impactPos);
            if (distance < radius) {
                float adjustedDamage = damage * (1.0f - (distance / radius));  // Scale damage by distance
                DamageFireCell(vertex, adjustedDamage);
            }
        }
    } else {
        FireVertex NearestVertex = FindNearestVertex(impactPos);
        if (Utils::GetWorldPosition(NearestVertex).GetDistance(impactPos) > 128.0f) {
            return;
        }
        DamageFireCell(NearestVertex, damage);
    }
}

bool WildfireMgr::IsCellAltered(RE::TESObjectCELL* cell) {
    auto fireCell = GetOrCreateFireCellState(cell);
    if (fireCell->altered) {
        return true;  // If any vertex in the cell has been altered by fire
    }
    return false;
}

void WildfireMgr::DamageFireCell(const FireVertex target, float damage, bool mgr) {
    auto* set = Settings::GetSingleton();
    FireCellState* cellState = GetOrCreateFireCellState(target.cell);
    int quadrant = target.quadrant;
    int vertexIndex = target.vertex;

    if (cellState->fuel[quadrant][vertexIndex] <= 0.0f || !cellState->canBurn[quadrant][vertexIndex] ||
        cellState->isCharred[quadrant][vertexIndex]) {
        // vertex adjusted to vertex with grass sometimes have grass
        if (mgr) {
            auto& colors = target.cell->GetRuntimeData().cellLand->loadedData->colors[quadrant][vertexIndex];
            if (colors[0] > 15 || colors[1] > 15 || colors[2] > 15) {
                colors[0] -= 15;  // R
                colors[1] -= 15;  // G
                colors[2] -= 15;  // B
                // Mark the cell as altered by fire
                cellState->altered = true;
            } else if (colors[0] != 0 || colors[1] != 0 || colors[2] != 0) {
                colors[0] -= 0;  // R
                colors[1] -= 0;  // G
                colors[2] -= 0;  // B
                // Mark the cell as altered by fire
                cellState->altered = true;
            }
        }
        return;
    }  // If no fuel, can't burn, or already charred, do nothing

    cellState->heat[quadrant][vertexIndex] += damage;

    if (!cellState->isBurning[quadrant][vertexIndex]) { // If not already burning, check if it should start burning

        if (cellState->heat[quadrant][vertexIndex] / cellState->minBurnHeat[quadrant][vertexIndex] >= 1.0f) {  
            cellState->isBurning[quadrant][vertexIndex] = true;

            auto HazardMgr = HazardMgr::GetSingleton();
            float HazardLifetime = cellState->fuel[quadrant][vertexIndex] / set->FuelConsumptionRate;
            HazardMgr->SpawnHazardAtVertex(target, HazardMgr->FireDragonHazard, HazardLifetime);

        } else {
            auto& colors = target.cell->GetRuntimeData().cellLand->loadedData->colors[quadrant][vertexIndex];
            float heatRatio = cellState->heat[quadrant][vertexIndex] / cellState->minBurnHeat[quadrant][vertexIndex];
            uint8_t colorValue = static_cast<uint8_t>(128.0f + (128.0f * (1.0f - heatRatio)));
            if (colors[0] > colorValue || colors[1] > colorValue || colors[2] > colorValue) {
                colors[0] = colorValue;  // R
                colors[1] = colorValue;  // G
                colors[2] = colorValue;  // B
                // Mark the cell as altered by fire
                cellState->altered = true;
            }
        }
    }

}

void WildfireMgr::CoolFireCell(FireVertex target, float damage) {
    FireCellState* cellState = GetOrCreateFireCellState(target.cell);
    int quadrant = target.quadrant;
    int vertexIndex = target.vertex;

    if (cellState->fuel[quadrant][vertexIndex] <= 0 || !cellState->canBurn[quadrant][vertexIndex] ||
        cellState->isCharred[quadrant][vertexIndex]) {
        return;
    }  // If no fuel, can't burn, or already charred, do nothing

    if (cellState->heat[quadrant][vertexIndex] <= 0) {
        return;  // If no heat, nothing to cool down
    }
    cellState->heat[quadrant][vertexIndex] -= damage;
}

FireVertex WildfireMgr::FindNearestVertex(const RE::NiPoint3& pos) {
    auto* tes = RE::TES::GetSingleton();
    if (!tes) {
        return FireVertex{nullptr, 0, 0};
    }
    RE::TESObjectCELL* cell = tes->GetCell(pos);
    if (!cell) {
        return FireVertex{nullptr, 0, 0};
    }
    auto* land = cell->GetRuntimeData().cellLand;
    if (!land) {
        return FireVertex{nullptr, 0, 0};
    }

    // Local position within the cell (0 to 4096)
    float localX = pos.x - (cell->GetCoordinates()->cellX * 4096);
    float localY = pos.y - (cell->GetCoordinates()->cellY * 4096);

    // Determine quadrant:
    // Quadrants are split like:
    //  0 | 1
    // ---+---
    //  2 | 3
    // Determine quadrant
    int quadrantX = (localX >= 2048) ? 1 : 0;
    int quadrantY = (localY >= 2048) ? 1 : 0;
    int quadrant = quadrantY * 2 + quadrantX;

    // Convert to quadrant-local coordinates
    float quadLocalX = localX - (quadrantX * 2048);
    float quadLocalY = localY - (quadrantY * 2048);

    // Vertex index in 17x17 grid
    int vertX = std::clamp(static_cast<int>(std::round(quadLocalX / 128.0f)), 0, 16);
    int vertY = std::clamp(static_cast<int>(std::round(quadLocalY / 128.0f)), 0, 16);
    int vertIndex = vertY * 17 + vertX;

    FireVertex key{cell, quadrant, vertIndex};
    return key;
}


std::vector<FireVertex> WildfireMgr::FindNearestVertexsInRadius(const RE::NiPoint3& pos, float radius) {
    std::vector<FireVertex> result;

    FireVertex centerVertex = FindNearestVertex(pos);
    if (!centerVertex.cell) return result;

    auto [centerCellX, centerCellY] = GetCellCoords(centerVertex.cell);

    // Scan center cell and its 8 neighbors
    for (int cellDX = -1; cellDX <= 1; ++cellDX) {
        for (int cellDY = -1; cellDY <= 1; ++cellDY) {
            RE::TESObjectCELL* cell = GetCellByCoords(centerCellX + cellDX, centerCellY + cellDY);
            for (int q = 0; q < 4; ++q) {
                for (int v = 0; v < 289; ++v) {
                    FireVertex candidate{cell, q, v};
                    RE::NiPoint3 candidatePos = Utils::GetWorldPosition(candidate);
                    if (candidatePos.GetDistance(pos) <= radius) {
                        result.push_back(candidate);
                    }
                }
            }
        }
    }

    return result;
}

std::vector<WeightedNeighbour> WildfireMgr::GetFireVertexNeighboursWeighted(const FireVertex& vertex,
                                                                            WindData windData) {
    constexpr int VERTS_PER_QUAD = 17;
    std::vector<WeightedNeighbour> result;
    if (!vertex.cell) return result;

    int x = vertex.vertex % VERTS_PER_QUAD;
    int y = vertex.vertex / VERTS_PER_QUAD;

    // Directions: dx, dy, base weight
    const std::vector<std::tuple<int, int, float>> directions = {
        {0, -1, 1.0f},  // N
        {1, 0, 1.0f},   // E
        {0, 1, 1.0f},   // S
        {-1, 0, 1.0f},  // W
        {1, -1, 0.7f},  // NE
        {1, 1, 0.7f},   // SE
        {-1, 1, 0.7f},  // SW
        {-1, -1, 0.7f}  // NW
    };

    for (const auto& [dx, dy, baseWeight] : directions) {
        int nx = x + dx;
        int ny = y + dy;
        FireVertex neighbour = vertex;

        // Use your existing logic to get the correct neighbour (quadrant/cell transition)
        // For simplicity, use GetFireVertexNeighbours and filter by dx/dy if needed
        // Or copy the logic from your GetFireVertexNeighbours

        // Calculate wind effect
        float windEffect = 1.0f;
        if (windData.speed > 0.0f) {

            /*
            float windLen = std::sqrt(windDir.x * windDir.x + windDir.y * windDir.y);
            if (windLen > 0.0f) {
                float dirX = static_cast<float>(dx);
                float dirY = static_cast<float>(dy);
                float dirLen = std::sqrt(dirX * dirX + dirY * dirY);
                if (dirLen > 0.0f) {
                    // Dot product for alignment
                    float dot = (dirX * windDir.x + dirY * windDir.y) / (dirLen * windLen);
                    windEffect += windForce * dot;  // windForce scales the effect
                }
            }
            */
        }

        float finalWeight = baseWeight * windEffect;

        // Get actual neighbour vertex
        std::vector<FireVertex> nvec = GetFireVertexNeighbours(vertex);
        for (const auto& n : nvec) {
            int nx2 = n.vertex % VERTS_PER_QUAD;
            int ny2 = n.vertex / VERTS_PER_QUAD;
            if (nx2 == nx && ny2 == ny) {
                result.push_back({n, finalWeight});
                break;
            }
        }
    }

    // Sort by weight descending
    std::sort(result.begin(), result.end(),
              [](const WeightedNeighbour& a, const WeightedNeighbour& b) { return a.weight > b.weight; });

    return result;
}

FireCellState* WildfireMgr::GetOrCreateFireCellState(RE::TESObjectCELL* cell) {
    auto it = fireCellMap.find(cell);
    if (it != fireCellMap.end()) {
        return &(it->second);
    }
    auto [newIt, _] = fireCellMap.emplace(cell, FireCellState(cell));
    return &(newIt->second);
}

std::pair<int, int> WildfireMgr::GetCellCoords(RE::TESObjectCELL* cell) {
    auto coords = cell->GetCoordinates();
    return {coords->cellX, coords->cellY};
}

RE::TESObjectCELL* WildfireMgr::GetCellByCoords(int cellX, int cellY) {
    auto tes = RE::TES::GetSingleton();
    float worldX = cellX * 4096.0f;
    float worldY = cellY * 4096.0f;
    return tes->GetCell(RE::NiPoint3{worldX, worldY, 0.0f});
}

std::vector<FireVertex> WildfireMgr::GetFireVertexNeighbours(const FireVertex& vertex) {
    constexpr int VERTS_PER_QUAD = 17;
    constexpr int QUADS_PER_ROW = 2;
    std::vector<FireVertex> result;
    if (!vertex.cell) return result;

    // Convert vertex index to (x, y) in the quadrant grid
    int x = vertex.vertex % VERTS_PER_QUAD;
    int y = vertex.vertex / VERTS_PER_QUAD;

    // Convert quadrant to (qx, qy)
    int qx = vertex.quadrant % QUADS_PER_ROW;
    int qy = vertex.quadrant / QUADS_PER_ROW;

    auto [cellX, cellY] = GetCellCoords(vertex.cell);

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;  // Exclude center

            int nx = x + dx;
            int ny = y + dy;
            int nqx = qx;
            int nqy = qy;
            int ncellX = cellX;
            int ncellY = cellY;

            // Handle crossing quadrant boundaries
            if (nx < 0) {
                nqx -= 1;
                nx = VERTS_PER_QUAD - 1;
            } else if (nx >= VERTS_PER_QUAD) {
                nqx += 1;
                nx = 0;
            }
            if (ny < 0) {
                nqy -= 1;
                ny = VERTS_PER_QUAD - 1;
            } else if (ny >= VERTS_PER_QUAD) {
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

            int nquad = nqy * 2 + nqx;
            RE::TESObjectCELL* ncell = vertex.cell;
            if (ncellX != cellX || ncellY != cellY) {
                ncell = GetCellByCoords(ncellX, ncellY);
                if (!ncell) continue;
            }

            int nvertex = ny * VERTS_PER_QUAD + nx;
            result.push_back(FireVertex{ncell, nquad, nvertex});
        }
    }
    return result;
}

std::pair<uint8_t, uint8_t> WildfireMgr::GetCurrentWind() {
    auto sky = RE::Sky::GetSingleton();
    if (!sky || !sky->currentWeather) {
        return {0, 0};
    }
    auto& data = sky->currentWeather->data;
    uint8_t windSpeed = data.windSpeed;
    uint8_t windDirection = data.windDirection;
    return {windSpeed, windDirection};
}

bool WildfireMgr::IsCurrentWeatherRaining() {
    auto sky = RE::Sky::GetSingleton();
    if (!sky || !sky->currentWeather) {
        return false;
    }
    const auto& weatherData = sky->currentWeather->data;
    bool rain = weatherData.flags.any(RE::TESWeather::WeatherDataFlag::kRainy);
    bool snow = weatherData.flags.any(RE::TESWeather::WeatherDataFlag::kSnow);

    return rain || snow;
}


void WildfireMgr::ResetFireCellState(RE::TESObjectCELL* cell) {
    if (cell) {
        FireCellState* fireCell = GetOrCreateFireCellState(cell);
        if (auto& cellLand = cell->GetRuntimeData().cellLand) {
            if (auto& loadedData = cellLand->loadedData) {
                if (auto& colors = loadedData->colors) {
                    auto& OrgColors = fireCell->originalColors;
                    std::memcpy(colors, OrgColors, sizeof(OrgColors));
                }
            }
        }
    }

    fireCellMap.erase(cell);
}

void WildfireMgr::ResetAllFireCells() { 
    std::queue<RE::TESObjectCELL*> cellsToReset;
    for (const auto& [cell, state] : fireCellMap) {
        cellsToReset.push(cell);
    }
    
    while (!cellsToReset.empty()) {
        RE::TESObjectCELL* cell = cellsToReset.front();
        cellsToReset.pop();
        ResetFireCellState(cell);
    }
}
