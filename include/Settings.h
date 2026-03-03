#pragma once 

#include "REX/REX.h"

class Settings : public REX::Singleton<Settings> {
public:
    Settings() { LoadSettings(); }

    void LoadSettings();
    void SaveSettings() const;
    void ResetSettings();

    std::vector<std::string> fireSources;
    std::vector<std::string> shockSources;
    std::vector<std::string> coldSources;
    std::vector<std::string> waterSources;

    bool ModActive = true;   // Kill Switch
    bool DebugMode = false;  // Enable debug mode (visuals)
    float DebugDrawDuration = 1000.0f;  // Duration for debug drawings in miliseconds

    float GrassPeriodicUpdateTime = 1.0f;   // Time in seconds between periodic updates
    float HazardPeriodicUpdateTime = 0.1f;  // Time in seconds between periodic hazard updates
    float RefPeriodicUpdateTime = 1.0f;     // Time in seconds between periodic tree updates

    float DefaultMinHeatToBurn = 25.0f;      // Minimum heat required for a cell to start burning
    float DefaultInitialFuelAmount = 25.0f;  // Initial fuel amount for each vertex
    float DefaultExplosionDamage = 50.0f;    // Default damage from explosions if not specified in the explosion form

    float HeatDistributionFactor = 10.0f;    // Heat distribution factor for fire spread
    float FuelConsumptionRate = 1.0f;        // Rate at which fuel is consumed per second
    float FuelToHeatRate = 0.50f;            // Rate at which fuel contributes to heat generation
    float SelfHeatLoss = 0.10f;              // Heat loss per second for non-burning cells
    float SelfColdLoss = 0.50f;              // Cold loss per second for non-burning cells
    float RefDamagePerBurningVertex = 5.0f;  // Damage to refs per burning vertex within bounds
    uint8_t MinGrassColorValue = 45;         // Minimum color value for grass vertices to

    float FireDamageMultiplayer = 1.0f;   // Multiplier for damage from projectiles
    float ShockDamageMultiplayer = 0.75f;  // Multiplier for damage from projectiles
    float ColdDamageMultiplayer = 1.5f;   // Multiplier for damage from projectiles
    float WaterDamageMultiplayer = 1.0f;  // Multiplier for damage from projectiles

    float RainingFactor = 0.60f;   // Factor to reduce fire spread when raining
    float SnowingFactor = 0.75f;   // Factor to reduce fire spread when snowing
    float WindSpeedFactor = 2.0f;  // Factor to influence fire spread based on wind speed

    int MaxHazardSpawn = 25;  // Maximum hazards to spawn
};