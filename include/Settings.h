#pragma once 

#include "REX/REX.h"

#include "Types.h"

#include <unordered_set>

class Settings : public REX::Singleton<Settings> {
public:
    Settings() { LoadSettings(); }

    void LoadSettings();        // Load Settings from file
    void SaveSettings() const;  // Save settings to file
    void ResetSettings();       // Restore default and save to file


    // Grass and Refs
    std::unordered_map<RE::FormID, GrassConfig> grassConfig;
    std::unordered_map<RE::FormID, RefConfig> refsConfig;
    std::unordered_set<RE::FormID> excludedTrees;

    // Sources
    std::unordered_map<RE::FormID, ElementType> sourceForms;

    // String configs
    std::vector<std::string> fireSources;
    std::vector<std::string> shockSources;
    std::vector<std::string> coldSources;
    std::vector<std::string> waterSources;

    // Kill Switch
    bool ModActive = true;

    // Debug
    bool DebugMode = false;  // Enable debug mode (visuals)
    bool DrawImpacts = true;
    bool DrawLandscape = true;
    bool DrawBoundBoxes = true;
    bool DrawHazards = true;
    int DebugDrawDuration = 1000;  // Duration for debug drawings in miliseconds

    // Timers
    float GrassPeriodicUpdateTime = 1.0f;   // Time in seconds between periodic updates
    float HazardPeriodicUpdateTime = 1.5f;  // Time in seconds between periodic hazard updates
    float RefPeriodicUpdateTime = 1.0f;     // Time in seconds between periodic tree updates

    // Grass
    float DefaultGrassIgnitionHeat = 25.0f;  // Minimum heat required for a cell to start burning
    float DefaultGrassFuel = 25.0f;          // Initial fuel amount for each vertex
    float DefaultMaxGrassHeat = 100.0f;      // Max heat grass can have
    float DefaultMinGrassHeat = -50.0f;      // Min heat grass can have
    uint8_t MinGrassColorValue = 45;         // Minimum color value for grass vertices to

    // Refs
    float DefaultRefIgnitionHeat = 50.0f;  // Minimum heat required for a cell to start burning
    float DefaultRefFuel = 50.0f;          // Initial fuel amount for each vertex
    float DefaultMaxRefHeat = 250.0f;      // Max heat ref can have
    float DefaultMinRefHeat = -100.0f;     // Min heat ref can have

    // Fire spreading
    float HeatDistributionFactor = 10.0f;  // Higher number = slower distribution
    float MaxHeatDistribution = 5.0f;      // Max heat that can be distributed at once
    float FuelConsumptionRate = 1.0f;      // Rate at which fuel is consumed per second
    float FuelToHeatRate = 0.50f;          // Rate at which fuel contributes to heat generation
    float SelfHeatLoss = 0.10f;            // Heat loss per second for non-burning cells
    float SelfColdLoss = 0.50f;            // Cold loss per second for non-burning cells

    // Damage Multiplayers
    float FireDamageMultiplayer = 1.0f;   // Multiplier for damage from projectiles
    float ShockDamageMultiplayer = 0.5f;  // Multiplier for damage from projectiles
    float ColdDamageMultiplayer = 1.5f;   // Multiplier for damage from projectiles
    float WaterDamageMultiplayer = 1.0f;  // Multiplier for damage from projectiles

    // Wheather
    float RainingFactor = 0.60f;   // Factor to reduce fire spread when raining
    float SnowingFactor = 0.75f;   // Factor to reduce fire spread when snowing
    float WindSpeedFactor = 2.0f;  // Factor to influence fire spread based on wind speed

    // Hazards
    int MaxHazardSpawn = 25;  // Maximum hazards to spawn
    bool HazardAffectPCOnly = true;
};