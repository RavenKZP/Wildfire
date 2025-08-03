#include "Types.h"
#include "Settings.h"
#include "Utils.h"

FireCellState::FireCellState(RE::TESObjectCELL* cell) {
    auto cellLand = cell->GetRuntimeData().cellLand;
    auto& colors = cellLand->loadedData->colors;

    std::memcpy(originalColors, colors, sizeof(colors));
    std::memset(heat, 0, sizeof(heat));
    std::memset(isBurning, false, sizeof(isBurning));
    std::memset(isCharred, false, sizeof(isCharred));

    for (int q = 0; q < 4; ++q) {
        for (int v = 0; v < 289; ++v) {
            auto [canBurnValue, fuelValue, minBurnHeatValue] = Utils::GetGrassData(cellLand->loadedData, q, v);

            canBurn[q][v] = canBurnValue;
            fuel[q][v] = fuelValue;
            minBurnHeat[q][v] = minBurnHeatValue;
        }
    }
    altered = false;
}