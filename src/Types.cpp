#include "Types.h"

#include "Settings.h"
#include "Utils.h"

FireVertex::FireVertex(CellAsCoords cell, int quadrant, int vertex) {
    this->cell = cell;
    this->quadrant = quadrant;
    this->vertex = vertex;
    // Calculate world position based on cell and vertex indices

    // Cell world origin
    float cellWorldX = float(cell.cellX) * CELL_SIZE;
    float cellWorldY = float(cell.cellY) * CELL_SIZE;
    // Quadrant (qx, qy)
    int qx = quadrant % QUADS_PER_ROW;
    int qy = quadrant / QUADS_PER_ROW;
    // Quadrant world offset
    float quadWorldX = cellWorldX + qx * QUAD_SIZE;
    float quadWorldY = cellWorldY + qy * QUAD_SIZE;
    // Vertex (vx, vy) in quadrant
    int vx = vertex % VERTS_PER_QUAD_ROW;
    int vy = vertex / VERTS_PER_QUAD_ROW;
    // Vertex world offset
    float vertWorldX = quadWorldX + vx * VERTS_STEP;
    float vertWorldY = quadWorldY + vy * VERTS_STEP;
    float vertWorldZ = 0.0f;
    // Get land height at vertex position
    RE::TES::GetSingleton()->GetLandHeight(RE::NiPoint3{vertWorldX, vertWorldY, 0.0f}, vertWorldZ);

    worldsPos = RE::NiPoint3{vertWorldX, vertWorldY, vertWorldZ};
}

// Never ever call this outside of the main thread
CellState::CellState(RE::TESObjectCELL* cell) {
    auto start = std::chrono::high_resolution_clock::now();

    valid = false; // assume invalid
    std::memset(canBurn, false, sizeof(canBurn));
    std::memset(minBurnHeat, 0, sizeof(minBurnHeat));
    std::memset(fuel, 0, sizeof(fuel));
    std::memset(originalColors, 0, sizeof(originalColors));
    std::memset(newColors, 0, sizeof(newColors));
    std::memset(heat, 0, sizeof(heat));
    std::memset(usedHeat, 0, sizeof(usedHeat));
    std::memset(isBurning, false, sizeof(isBurning));
    std::memset(isCharred, false, sizeof(isCharred));
    std::memset(isUnderwater, false, sizeof(isUnderwater));
    std::memset(altered, false, sizeof(altered));

    if (!cell) {
        logger::error("CellState initialized with null cell pointer");
        return;
    }

    auto coord = cell->GetCoordinates();
    if (coord) {
        cellAsCoordinates = CellAsCoords{coord->cellX, coord->cellY};
    } else {
        logger::error("CellState initialized with interior cell");
    }

    if (!cell->GetRuntimeData().cellLand || !cell->GetRuntimeData().cellLand->loadedData) {
        logger::error("CellState initialized with cell that has no loaded land data");
        return;
    }
    auto cellLand = cell->GetRuntimeData().cellLand;
    if (!cellLand->loadedData->colors) {
        logger::error("CellState initialized with cell that has no color data");
        return;
    }

    auto& loadedData = cellLand->loadedData;
    auto& colors = cellLand->loadedData->colors;

    std::memcpy(originalColors, colors, sizeof(colors));
    std::memcpy(newColors, colors, sizeof(colors));

    auto* set = Settings::GetSingleton();
    for (int q = 0; q < CELL_QUAD_COUNT; ++q) {
        for (int v = 0; v < VERTS_PER_QUAD; ++v) {
            bool canBurnVal = false;
            float fuelVal = set->DefaultInitialFuelAmount;
            float minBurnHeatVal = set->DefaultMinHeatToBurn;

            // Check if vertex is underwater
            auto vertPos = FireVertex(cellAsCoordinates, q, v).worldsPos;
            auto waterHeight = RE::TES::GetSingleton()->GetWaterHeight(vertPos, cell);
            if (vertPos.z < waterHeight) {
                isUnderwater[q][v] = true;
                canBurn[q][v] = false;
                continue;  // Skip burnable checks for underwater vertices
            }

            float defaultTexturePercent = 1.0f;

            for (int texIdx = 0; texIdx < 7; ++texIdx) {  // 6 quad textures + 1 default texture
                // Check percent coverage
                float percent = 0.0f;
                if (texIdx < 6) {  // Quad texture
                    percent = (static_cast<float>(loadedData->percents[q][v][texIdx])) / 255.0f;
                    defaultTexturePercent -= percent;
                } else {  // Default texture
                    percent = defaultTexturePercent;
                }
                if (percent > 0.0f) {
                    RE::TESLandTexture* tex = loadedData->defQuadTextures[q];
                    if (texIdx < 6) {
                        tex = loadedData->quadTextures[q][texIdx];
                    }
                    if (!tex) continue;

                    // Check material type
                    if (tex->materialType->materialID == RE::MATERIAL_ID::kGrass) {
                        canBurnVal = true;
                    }

                    // Check grass list
                    for (const auto& grass : tex->textureGrassList) {
                        if (grass) {
                            // If any grass type is burnable, the vertex can burn
                            canBurnVal = true;  // Temporary solution
                            // fuelVal = ;
                            // minBurnHeatVal = ;
                        }
                    }
                }
            }
            canBurn[q][v] = canBurnVal;
            fuel[q][v] = fuelVal;
            minBurnHeat[q][v] = minBurnHeatVal;
        }
    }

    valid = true;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    logger::debug("Initialized CellState for cell ({}, {}) in {} ms", cellAsCoordinates.cellX,
                 cellAsCoordinates.cellY, elapsed.count());
}

RefState::RefState(RE::TESObjectREFR* ref) {
    heat = 0.0f;
    initialFuel = 50.0f;
    minBurnHeat = 50.0f;  // Temporary default values
    fuel = 50.0f;
    halfEvent = false;
    isBurning = false;
    isCharred = false;
    isFrozen = false;
    if (!ref) {
        return;
    }
    position = ref->GetPosition();
    angle = ref->GetAngle();
    BBox = Utils::GetBoundingBox(ref);
}