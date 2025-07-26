#include "WildfireMgr.h"
#include "Settings.h"
#include "Utils.h"

void WildfireMgr::PeriodicUpdate(float delta) {
    auto* set = Settings::GetSingleton();
    for (auto& fireCell : fireCellMap) {
        if (fireCell.second.altered) {
            using RemoveGrassInCell = void (*)(RE::BGSGrassManager*, RE::TESObjectCELL*);
            REL::Relocation<RemoveGrassInCell> RemoveGrassInCellfunc{REL::RelocationID(15207, 15375)};
            RemoveGrassInCellfunc(RE::BGSGrassManager::GetSingleton(), fireCell.first);

            using CreateGrassInCell = void (*)(RE::TESObjectCELL*);
            REL::Relocation<CreateGrassInCell> CreateGrassInCellfunc{REL::RelocationID(13137, 13277)};
            CreateGrassInCellfunc(fireCell.first);

            fireCell.second.altered = false;  // Reset altered state after processing
        }
        for (int q = 0; q < 4; ++q) {
            for (int v = 0; v < 289; ++v) {
                auto& colors = fireCell.first->GetRuntimeData().cellLand->loadedData->colors[q][v];
                if (fireCell.second.isBurning[q][v]) {
                    // ToDo: Get Wind Force and direction
                    // ToDo: Neighbours veight (0.7 for diagonal, 1.0 for cardinal directions)

                    FireVertex targetVertex{fireCell.first, q, v};

                    auto neighbours = GetFireVertexNeighbours(targetVertex);

                    for (const auto& neighbour : neighbours) {
                        // Damage neighbouring cells
                        DamageFireCell(neighbour, fireCell.second.heat[q][v] / set->HeatDistributionFactor);
                    }

                    // Decrease heat based on neighbours
                    fireCell.second.heat[q][v] -=
                        neighbours.size() * (fireCell.second.heat[q][v] / set->HeatDistributionFactor) * delta;

                    // Decrease fuel amount
                    fireCell.second.fuel[q][v] -= set->FuelConsumptionRate * delta;
                    // Heat increases as fuel burns
                    fireCell.second.heat[q][v] +=
                        set->FuelConsumptionRate * set->FuelToHeatRate * delta;

                    // Mark as charred when burning stops
                    if (fireCell.second.fuel[q][v] <= 0.0f) {
                        fireCell.second.isBurning[q][v] = false;
                        fireCell.second.isCharred[q][v] = true;

                        colors[0] = 0.0f;  // R
                        colors[1] = 0.0f;  // G
                        colors[2] = 0.0f;  // B
                        // Mark the Cell as altered by fire
                        fireCell.second.altered = true;
                    } else {
                        // Update color based on fuel left
                        float fuelRatio = fireCell.second.fuel[q][v] / set->InitialFuelAmount;
                        float colorValue = 127.0f - (127.0f * (1.0f - fuelRatio));
                        if (colors[0] > colorValue || colors[1] > colorValue || colors[2] > colorValue ||
                            colors[0] < 0 || colors[1] < 0 || colors[2] < 0) {
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
	FireVertex NearestVertex = FindNearestVertex(impactPos);
    if (!NearestVertex.cell) {
        logger::error("Failed to find nearest vertex for fire event at position ({}, {}, {})", impactPos.x, impactPos.y,
                      impactPos.z);
        return;
    }
    // ToDo: Damage all vertices within the radius
    DamageFireCell(NearestVertex, damage);
}

bool WildfireMgr::IsCellAltered(RE::TESObjectCELL* cell) {
    auto fireCell = GetOrCreateFireCellState(cell);
    if (fireCell->altered) {
        return true;  // If any vertex in the cell has been altered by fire
    }
    return false;
}

void WildfireMgr::DamageFireCell(const FireVertex target, float damage) {
    auto* set = Settings::GetSingleton();
    FireCellState* cellState = GetOrCreateFireCellState(target.cell);
    int quadrant = target.quadrant;
    int vertexIndex = target.vertex;

    if (cellState->fuel[quadrant][vertexIndex] <= 0 || !cellState->canBurn[quadrant][vertexIndex] ||
        cellState->isCharred[quadrant][vertexIndex]) {
        return;
    }  // If no fuel, can't burn, or already charred, do nothing

    cellState->heat[quadrant][vertexIndex] += damage;

    if (!cellState->isBurning[quadrant][vertexIndex]) { // If not already burning, check if it should start burning

        if (cellState->heat[quadrant][vertexIndex] / set->MinHeatToBurn >= 1.0f) {  
            cellState->isBurning[quadrant][vertexIndex] = true;

        } else {
            auto& colors = target.cell->GetRuntimeData().cellLand->loadedData->colors[quadrant][vertexIndex];
            float heatRatio = cellState->heat[quadrant][vertexIndex] / set->MinHeatToBurn;
            float colorValue = 127.0f + (127.0f * (1.0f - heatRatio));
            if (colors[0] > colorValue || colors[1] > colorValue || colors[2] > colorValue || 
                colors[0] < 0 || colors[1] < 0 || colors[2] < 0) {
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
    int qx = vertex.quadrant % 2;
    int qy = vertex.quadrant / 2;

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
