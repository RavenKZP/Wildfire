#pragma once

#include "ClibUtil/singleton.hpp"

class Settings : public clib_util::singleton::ISingleton<Settings> {
public:
    void LoadSettings();
    void SaveSettings() const;
    void ResetSettings();

    // Kill Switch
    bool ModActive = true;
    bool DebugMode = false;

    float MinHeatToBurn = 25.0f;            // Minimum heat required for a cell to start burning
    float PeriodicUpdateTime = 1.0f;        // Time in seconds between periodic updates
    float HeatDistributionFactor = 10.0f;  // Heat distribution factor for fire spread
    float FuelConsumptionRate = 3.0f;       // Rate at which fuel is consumed per update
    float InitialFuelAmount = 50.0f;        // Initial fuel amount for each vertex
    float FuelToHeatRate = 0.20f;            // Rate at which fuel contributes to heat generation
    float SelfHeatLoss = 0.1f;              // Heat loss per update for non-burning cells
    float HeatToBigFire = 100.0f;            // Heat threshold for a cell to become a big fire
    float DamageMultiplayer = 1.0f;         // Multiplier for damage from projectiles

};
