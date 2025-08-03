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

    // Kill Switch
    bool ModActive = true;
    bool DebugMode = false;

    float MinHeatToBurn = 25.0f;           // Minimum heat required for a cell to start burning
    float PeriodicUpdateTime = 1.0f;       // Time in seconds between periodic updates
    float HeatDistributionFactor = 10.0f;  // Heat distribution factor for fire spread
    float FuelConsumptionRate = 3.0f;      // Rate at which fuel is consumed per second
    float InitialFuelAmount = 50.0f;       // Initial fuel amount for each vertex
    float FuelToHeatRate = 0.20f;          // Rate at which fuel contributes to heat generation
    float SelfHeatLoss = 0.10f;            // Heat loss per second for non-burning cells
    float DamageMultiplayer = 1.0f;        // Multiplier for damage from projectiles
    float RainingFactor = 0.75f;           // Factor to reduce fire spread when raining
};
