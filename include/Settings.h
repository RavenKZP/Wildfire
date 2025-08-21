#pragma once

#include "Types.h"

#include "ClibUtil/singleton.hpp"
#include <nlohmann/json.hpp>
#include <unordered_set>

class Settings : public clib_util::singleton::ISingleton<Settings> {
public:
    void LoadSettings();
    void SaveSettings() const;
    void ResetSettings();

    std::vector<GrassFireConfig> grassConfigs;
    std::vector<std::string> fireSources;
    std::vector<std::string> coldSources;
    std::vector<std::string> waterSources;

    // Kill Switch
    bool ModActive = true;
    bool DebugMode = false;

    int GrassGenerationCellsPerFrameLimit = 1;  // Limit for grass generation per frame

    float GrassPeriodicUpdateTime = 1.0f;   // Time in seconds between periodic updates
    float HazardPeriodicUpdateTime = 1.0f;  // Time in seconds between periodic hazard updates

    float DefaultMinHeatToBurn = 25.0f;      // Minimum heat required for a cell to start burning
    float DefaultInitialFuelAmount = 50.0f;  // Initial fuel amount for each vertex
    float HeatDistributionFactor = 10.0f;    // Heat distribution factor for fire spread
    float FuelConsumptionRate = 3.0f;        // Rate at which fuel is consumed per second
    float FuelToHeatRate = 0.50f;            // Rate at which fuel contributes to heat generation
    float SelfHeatLoss = 0.10f;              // Heat loss per second for non-burning cells
    float FireDamageMultiplayer = 1.0f;      // Multiplier for damage from projectiles
    float ColdDamageMultiplayer = 1.5f;      // Multiplier for damage from projectiles
    float WaterDamageMultiplayer = 1.0f;     // Multiplier for damage from projectiles
    float RainingFactor = 0.75f;             // Factor to reduce fire spread when raining
    float WindSpeedFactor = 1.0f;            // Factor to influence fire spread based on wind speed
    float DefaultExplosionDamage = 50.0f;
    float DefaultDamage = 50.0f;
};
