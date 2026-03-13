#include "Types.h"

#include "Settings.h"
#include "Utils.h"

FireVertex::FireVertex(CellAsCoords cell, int quadrant, int vertex) {
    static auto* tes = RE::TES::GetSingleton();
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
    tes->GetLandHeight(RE::NiPoint3{vertWorldX, vertWorldY, 0.0f}, vertWorldZ);

    worldsPos = RE::NiPoint3{vertWorldX, vertWorldY, vertWorldZ};
}

// Never ever call this outside of the main thread
CellState::CellState(RE::TESObjectCELL* cell) {
    auto start = std::chrono::high_resolution_clock::now();
    static auto* set = Settings::GetSingleton();

    cellAsCoordinates = {0, 0};
    valid = false; // assume invalid
    std::memset(canBurn, false, sizeof(canBurn));
    std::memset(ignitionHeat, 0, sizeof(ignitionHeat));
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

    auto cellLand = cell->GetLandscape();
    if (!cellLand) {
        logger::error("CellState ({}, {}) initialized with cell that has no cellLand data", cellAsCoordinates.cellX,
                      cellAsCoordinates.cellY);
        return;
    }

    auto& loadedData = cellLand->loadedData;
    auto& colors = cellLand->loadedData->colors;

    std::memcpy(originalColors, colors, sizeof(colors));
    std::memcpy(newColors, colors, sizeof(colors));

    for (int q = 0; q < CELL_QUAD_COUNT; ++q) {
        for (int v = 0; v < VERTS_PER_QUAD; ++v) {
            bool canBurnVal = false;
            float fuelVal = set->DefaultGrassFuel;
            float ignitionHeatVal = set->DefaultGrassIgnitionHeat;

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
                    float totalFuel = 0.0f;
                    float totalIgnition = 0.0f;
                    int grassCount = 0;

                    for (const auto& grass : tex->textureGrassList) {
                        if (!grass) {
                            continue;
                        }

                        auto grassFormID = grass->GetFormID();
                        auto it = set->grassConfig.find(grassFormID);

                        if (it != set->grassConfig.end()) {
                            const auto& cfg = it->second;

                            if (cfg.canBurn) {
                                canBurnVal = true;

                                totalFuel += cfg.fuel;
                                totalIgnition += cfg.ignitionHeat;

                                grassCount++;
                            }
                        } else {
                            canBurnVal = true;
                            totalFuel += set->DefaultGrassFuel;
                            totalIgnition += set->DefaultGrassIgnitionHeat;
                            grassCount++;
                        }
                    }

                    if (grassCount > 0) {
                        fuelVal = totalFuel / grassCount;
                        ignitionHeatVal = totalIgnition / grassCount;
                    }
                }
            }
            canBurn[q][v] = canBurnVal;
            fuel[q][v] = fuelVal;
            ignitionHeat[q][v] = ignitionHeatVal;
        }
    }

    valid = true;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    logger::debug("Initialized CellState ({}, {}) in {} ms", cellAsCoordinates.cellX,
                 cellAsCoordinates.cellY, elapsed.count());
}

RefState::RefState(RE::TESObjectREFR* ref) {
    auto start = std::chrono::high_resolution_clock::now();
    static auto* set = Settings::GetSingleton();
    static auto* tes = RE::TES::GetSingleton();

    heat = 0.0f;
    fuel = set->DefaultRefFuel;
    initialFuel = fuel;
    ignitionHeat = set->DefaultRefIgnitionHeat;

    canBurn = false;
    isBurning = false;
    isCharred = false;
    isFrozen = false;
    isUnderwater = false;
    halfEvent = false;

    if (!ref) {
        return;
    }

    auto base = ref->GetBaseObject();
    if (!base) {
        return;
    }

    auto formID = base->GetFormID();

    // -------------------------
    // 1. Excluded Trees Check
    // -------------------------
    if (base->Is(RE::FormType::Tree)) {
        if (set->excludedTrees.contains(formID)) {
            return;
        }
        canBurn = true;
    }

    // -------------------------
    // 2. Config Overrides
    // -------------------------
    if (auto it = set->refsConfig.find(formID); it != set->refsConfig.end()) {
        const auto& cfg = it->second;

        canBurn = cfg.canBurn;
        fuel = cfg.fuel;
        initialFuel = fuel;
        ignitionHeat = cfg.ignitionHeat;
    }

    // -------------------------
    // 3. Transform / Bounding Box
    // -------------------------
    position = Utils::GetPosition(ref);
    angle = ref->GetAngle();
    BBox = Utils::GetBoundingBox(ref);

    // -------------------------
    // 4. Underwater Detection
    // -------------------------
    auto* cell = ref->GetParentCell();

    if (cell) {
        float minZ = BBox[0].z;
        float maxZ = BBox[0].z;

        for (const auto& v : BBox) {
            minZ = std::min(minZ, v.z);
            maxZ = std::max(maxZ, v.z);
        }

        auto waterHeight = tes->GetWaterHeight(position, cell);

        float totalHeight = maxZ - minZ;
        float aboveWaterHeight = maxZ - std::max(minZ, waterHeight);

        float ratio = aboveWaterHeight / totalHeight;

        constexpr float burnThreshold = 0.30f;

        if (ratio < burnThreshold) {
            isUnderwater = true;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    logger::debug("Initialized RefState {:X} | burn:{} fuel:{} underwater:{} in {} ms", ref->GetFormID(), canBurn, fuel,
                  isUnderwater, elapsed.count());
}