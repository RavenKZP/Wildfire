#include "Settings.h"
#include "Utils.h"

#include <nlohmann/json.hpp>
#include "ClibUtil/editorID.hpp"

static ElementType ParseElementType(const std::string& str) {
    std::string s = Utils::ToLower(str);

    if (s == "fire") return ElementType::Fire;
    if (s == "shock") return ElementType::Shock;
    if (s == "cold") return ElementType::Cold;
    if (s == "water") return ElementType::Water;

    return ElementType::Neutral;
}

std::string FormIDToString(RE::FormID formID) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        return "0x0~Unknown";
    }

    uint8_t highByte = (formID >> 24) & 0xFF;

    if (highByte == 0xFE) {  // ESL
        uint16_t eslIndex = (formID >> 12) & 0xFFF;
        uint16_t localID = formID & 0xFFF;

        for (auto* file : dataHandler->files) {
            if (file && file->IsLight() && file->smallFileCompileIndex == eslIndex) {
                return std::format("0x{:03X}~{}", localID, file->fileName);
            }
        }
    } else {  // regular plugin
        uint32_t localID = formID & 0xFFFFFF;

        for (auto* file : dataHandler->files) {
            if (file && file->compileIndex == highByte) {
                return std::format("0x{:06X}~{}", localID, file->fileName);
            }
        }
    }
    return std::format("0x{:08X}~Unknown", formID);
}

static RE::TESForm* GetFormFromString(const std::string& formString) {
    auto pos = formString.find('~');
    // If no '~', treat as EditorID
    if (pos == std::string::npos) {
        auto* form = RE::TESForm::LookupByEditorID(formString);
        return form;
    }

    std::string idStr = formString.substr(0, pos);
    std::string plugin = formString.substr(pos + 1);

    uint32_t formID = std::stoul(idStr, nullptr, 16);

    auto handler = RE::TESDataHandler::GetSingleton();
    if (!handler) return nullptr;

    const RE::TESFile* mod = handler->LookupModByName(plugin);
    if (!mod) {
        logger::error("Can't find mod: {}", plugin);
        return nullptr;
    }

    // ESL (light plugin)
    if (mod->IsLight()) {
        formID &= 0xFFF;  // only lower 12 bits
    }

    return handler->LookupForm(formID, plugin);
}

// Load substring patterns (one per line, '#' prefix = comment)
static void LoadPatternsFromFile(const std::filesystem::path& path, std::vector<std::string>& outPatterns) {
    std::ifstream in(path);
    if (!in.is_open()) {
        logger::warn("Failed to open pattern config '{}'", path.string());
        return;
    }
    logger::info("Loading patterns from '{}'", path.string());
    std::string line;
    while (std::getline(in, line)) {
        // trim whitespace
        auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        auto last = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(first, last - first + 1);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        outPatterns.push_back(Utils::ToLower(trimmed));  // store lowercase
    }
}

static std::string RefTypeToString(const RefType type) {
    switch (type) {
        case RefType::Tree: return "tree";
        case RefType::Bush: return "bush";
        case RefType::Melt: return "melt";
        case RefType::Disintegrate: return "desintegrate";
        case RefType::FloraCoveredUnburnable: return "floracoveredunburnable";
        case RefType::BaseObjSwap: return "baseobjectswap";
        default: return "static";
    }
}

static RefType ParseRefType(const std::string& str) {
    std::string s = Utils::ToLower(str);
    if (s == "tree") return RefType::Tree;
    if (s == "bush") return RefType::Bush;
    if (s == "melt") return RefType::Melt;
    if (s == "desintegrate") return RefType::Disintegrate;
    if (s == "floracoveredunburnable") return RefType::FloraCoveredUnburnable;
    if (s == "baseobjectswap") return RefType::BaseObjSwap;
    return RefType::Static;
}

// Load all pattern files in folder
static void LoadAllPatterns(const std::filesystem::path& folder, std::vector<std::string>& outPatterns) {
    if (!std::filesystem::exists(folder)) return;
    for (auto& entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file()) continue;
        LoadPatternsFromFile(entry.path(), outPatterns);
    }
}

// Parsowanie GrassConfig
static GrassConfig ParseGrassConfig(const nlohmann::json& j) {
    GrassConfig cfg{};
    if (j.contains("canBurn")) cfg.canBurn = j["canBurn"];
    if (j.contains("fuel")) cfg.fuel = j["fuel"];
    if (j.contains("ignitionHeat")) cfg.ignitionHeat = j["ignitionHeat"];
    return cfg;
}

static RefConfig ParseRefConfig(const nlohmann::json& j) {
    RefConfig cfg{};
    if (j.contains("canBurn")) cfg.canBurn = j["canBurn"];
    if (j.contains("fuel")) cfg.fuel = j["fuel"];
    if (j.contains("ignitionHeat")) cfg.ignitionHeat = j["ignitionHeat"];

    if (j.contains("type")) cfg.type = ParseRefType(j["type"]);

    if (j.contains("baseObjectSwap")) {
        std::string formStr = j["baseObjectSwap"];
        if (formStr.find('~') != std::string::npos) {
            auto* form = GetFormFromString(formStr);
            cfg.baseObjectSwap = form ? form->As<RE::TESBoundObject>() : nullptr;
        } else {
            cfg.baseObjectSwap = RE::TESForm::LookupByEditorID<RE::TESBoundObject>(formStr);
        }
    }
    return cfg;
}

// Loading GrassConfig
static void LoadGrassConfigFile(const std::filesystem::path& path, std::unordered_map<RE::FormID, GrassConfig>& configs) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    nlohmann::json j;
    file >> j;
    if (!j.is_array()) return;
    for (auto& entry : j) {
        if (!entry.contains("FormID")) continue;
        std::string formStr = entry["FormID"];
        auto* form = GetFormFromString(formStr);
        if (!form) continue;
        auto config = ParseGrassConfig(entry);
        auto EditorID = clib_util::editorID::get_editorID(form);
        config.EditorID = EditorID;
        configs[form->GetFormID()] = config;
    }
}

static std::unordered_map<RE::FormID, GrassConfig> LoadGrassConfigs(const std::filesystem::path& dir) {
    std::unordered_map<RE::FormID, GrassConfig> configs;
    if (!std::filesystem::exists(dir)) return configs;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".json") {
            LoadGrassConfigFile(entry.path(), configs);
        }
    }
    return configs;
}

// RefConfig
static void LoadRefConfigFile(const std::filesystem::path& path, std::unordered_map<RE::FormID, RefConfig>& configs) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    nlohmann::json j;
    file >> j;
    if (!j.is_array()) return;
    for (auto& entry : j) {
        if (!entry.contains("FormID")) continue;
        std::string formStr = entry["FormID"];
        auto* form = GetFormFromString(formStr);
        if (!form) continue;
        auto config = ParseRefConfig(entry);
        config.EditorID = clib_util::editorID::get_editorID(form);
        configs[form->GetFormID()] = config;
    }
}

static std::unordered_map<RE::FormID, RefConfig> LoadRefConfigs(const std::filesystem::path& dir) {
    std::unordered_map<RE::FormID, RefConfig> configs;
    if (!std::filesystem::exists(dir)) return configs;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".json") {
            LoadRefConfigFile(entry.path(), configs);
        }
    }
    return configs;
}

static void LoadSourceFile(const std::filesystem::path& path, std::unordered_map<RE::FormID, ElementType>& sources) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    nlohmann::json j;
    file >> j;

    if (!j.is_array()) return;

    for (auto& entry : j) {
        if (!entry.contains("FormID") || !entry.contains("Element")) continue;

        auto* form = GetFormFromString(entry["FormID"]);
        if (!form) continue;

        ElementType type = ParseElementType(entry["Element"]);

        sources[form->GetFormID()] = type;
    }
}

static std::unordered_map<RE::FormID, ElementType> LoadSourceConfigs(const std::filesystem::path& dir) {
    std::unordered_map<RE::FormID, ElementType> sources;

    if (!std::filesystem::exists(dir)) return sources;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".json") {
            LoadSourceFile(entry.path(), sources);
        }
    }

    return sources;
}

static void LoadFormSetFile(const std::filesystem::path& path, std::unordered_set<RE::FormID>& outSet) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    nlohmann::json j;
    file >> j;

    if (!j.is_array()) return;

    for (auto& entry : j) {
        if (!entry.contains("FormID")) continue;

        auto* form = GetFormFromString(entry["FormID"]);
        if (!form) continue;

        outSet.insert(form->GetFormID());
    }
}

static std::unordered_set<RE::FormID> LoadFormSet(const std::filesystem::path& dir) {
    std::unordered_set<RE::FormID> result;

    if (!std::filesystem::exists(dir)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".json") {
            LoadFormSetFile(entry.path(), result);
        }
    }

    return result;
}

static nlohmann::json RefConfigToJson(RE::FormID formID, const RefConfig& cfg) {
    nlohmann::json j;

    auto* form = RE::TESForm::LookupByID(formID);
    if (form) {
        j["FormID"] = FormIDToString(form->GetFormID());
    }

    j["canBurn"] = cfg.canBurn;
    j["fuel"] = cfg.fuel;
    j["ignitionHeat"] = cfg.ignitionHeat;
    j["type"] = RefTypeToString(cfg.type);

    if (cfg.baseObjectSwap) {
        j["baseObjectSwap"] = FormIDToString(cfg.baseObjectSwap->GetFormID());
    }

    j["comment"] = clib_util::editorID::get_editorID(form);

    return j;
}

void Settings::SaveCustomRefConfigs(const std::unordered_map<RE::FormID, RefConfig>& configs) {
    nlohmann::json root = nlohmann::json::array();

    for (auto& [formID, cfg] : configs) {
        if (!customRefsConfig.contains(formID)) continue;
        root.push_back(RefConfigToJson(formID, cfg));
    }

    std::ofstream file("Data\\SKSE\\Plugins\\Wildfire\\Refs\\Custom.json");
    file << root.dump(4);
}

void Settings::LoadIni() {
    std::ifstream file("Data\\SKSE\\Plugins\\Wildfire.ini");
    if (!file.is_open()) {
        logger::error("INI not found");
        return;
    }

    auto Trim = [](std::string value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return std::string{};
        }

        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    };

    auto ParseBool = [](const std::string& value, bool& result) {
        const auto normalized = Utils::ToLower(value);

        if (normalized == "true" || normalized == "1" || normalized == "yes") {
            result = true;
            return true;
        }

        if (normalized == "false" || normalized == "0" || normalized == "no") {
            result = false;
            return true;
        }

        return false;
    };

    std::string line;

    while (std::getline(file, line)) {
        line = Trim(line);

        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = Trim(line.substr(0, separator));
        auto value = Trim(line.substr(separator + 1));

        const auto comment = value.find_first_of("#;");
        if (comment != std::string::npos) {
            value = Trim(value.substr(0, comment));
        }

        try {
            bool boolValue = false;

            if (key == "bModActive" && ParseBool(value, boolValue)) {
                ModActive = boolValue;
            } else if (key == "bEnableTimeLogging" && ParseBool(value, boolValue)) {
                EnableTimeLogging = boolValue;
            } else if (key == "bDebugMode" && ParseBool(value, boolValue)) {
                DebugMode = boolValue;
            } else if (key == "bDrawImpacts" && ParseBool(value, boolValue)) {
                DrawImpacts = boolValue;
            } else if (key == "bDrawLandscape" && ParseBool(value, boolValue)) {
                DrawLandscape = boolValue;
            } else if (key == "bDrawBoundBoxes" && ParseBool(value, boolValue)) {
                DrawBoundBoxes = boolValue;
            } else if (key == "bDrawHazards" && ParseBool(value, boolValue)) {
                DrawHazards = boolValue;
            } else if (key == "bDrawHazardsMovement" && ParseBool(value, boolValue)) {
                DrawHazardsMovement = boolValue;
            } else if (key == "bDrawDecals" && ParseBool(value, boolValue)) {
                DrawDecals = boolValue;
            } else if (key == "bDrawWind" && ParseBool(value, boolValue)) {
                DrawWind = boolValue;
            } else if (key == "bHazardAffectPCOnly" && ParseBool(value, boolValue)) {
                HazardAffectPCOnly = boolValue;
            } else if (key == "iDebugDrawDuration") {
                DebugDrawDuration = std::stoi(value);
            } else if (key == "fGrassPeriodicUpdateTime") {
                GrassPeriodicUpdateTime = std::stof(value);
            } else if (key == "fHazardPeriodicUpdateTime") {
                HazardPeriodicUpdateTime = std::stof(value);
            } else if (key == "fRefPeriodicUpdateTime") {
                RefPeriodicUpdateTime = std::stof(value);
            } else if (key == "fDefaultGrassIgnitionHeat") {
                DefaultGrassIgnitionHeat = std::stof(value);
            } else if (key == "fDefaultGrassFuel") {
                DefaultGrassFuel = std::stof(value);
            } else if (key == "fDefaultMaxGrassHeat") {
                DefaultMaxGrassHeat = std::stof(value);
            } else if (key == "fDefaultMinGrassHeat") {
                DefaultMinGrassHeat = std::stof(value);
            } else if (key == "iMinGrassColorValue") {
                MinGrassColorValue = static_cast<uint8_t>(std::stoi(value));
            } else if (key == "fGrassHeightDecrease") {
                GrassHeightDecrease = std::stof(value);
            } else if (key == "fDefaultRefIgnitionHeat") {
                DefaultRefIgnitionHeat = std::stof(value);
            } else if (key == "fDefaultRefFuel") {
                DefaultRefFuel = std::stof(value);
            } else if (key == "fDefaultMaxRefHeat") {
                DefaultMaxRefHeat = std::stof(value);
            } else if (key == "fDefaultMinRefHeat") {
                DefaultMinRefHeat = std::stof(value);
            } else if (key == "fUnderwaterRatio") {
                UnderwaterRatio = std::stof(value);
            } else if (key == "fFuelRecoveryRate") {
                FuelRecoveryRate = std::stof(value);
            } else if (key == "iMaxHazardSpawn") {
                MaxHazardSpawn = std::stoi(value);
            } else if (key == "iMaxSmokeHazards") {
                MaxSmokeHazards = std::stoi(value);
            } else if (key == "fMinDacalSize") {
                MinDacalSize = std::stof(value);
            } else if (key == "fMaxDacalSize") {
                MaxDacalSize = std::stof(value);
            } else if (key == "iDecalSpacing") {
                DecalSpacing = std::stoi(value);
            } else if (key == "iDecalSamplingPoints") {
                DecalSamplingPoints = std::stoi(value);
            } else if (key == "fHeatDistributionFactor") {
                HeatDistributionFactor = std::stof(value);
            } else if (key == "fMaxHeatDistribution") {
                MaxHeatDistribution = std::stof(value);
            } else if (key == "fFuelConsumptionRate") {
                FuelConsumptionRate = std::stof(value);
            } else if (key == "fFuelToHeatRate") {
                FuelToHeatRate = std::stof(value);
            } else if (key == "fSelfHeatLoss") {
                SelfHeatLoss = std::stof(value);
            } else if (key == "fSelfColdLoss") {
                SelfColdLoss = std::stof(value);
            } else if (key == "fFireDamageMultiplayer") {
                FireDamageMultiplayer = std::stof(value);
            } else if (key == "fShockDamageMultiplayer") {
                ShockDamageMultiplayer = std::stof(value);
            } else if (key == "fColdDamageMultiplayer") {
                ColdDamageMultiplayer = std::stof(value);
            } else if (key == "fWaterDamageMultiplayer") {
                WaterDamageMultiplayer = std::stof(value);
            } else if (key == "fRainingFactor") {
                RainingFactor = std::stof(value);
            } else if (key == "fSnowingFactor") {
                SnowingFactor = std::stof(value);
            } else if (key == "fWindSpeedFactor") {
                WindSpeedFactor = std::stof(value);
            }
        } catch (const std::exception& exception) {
            logger::warn(
                "Invalid value '{}' for INI setting '{}': {}",
                value,
                key,
                exception.what());
        }
    }

    logger::info("Settings read from INI");
}

void Settings::SaveIni() const {
    std::ofstream file("Data\\SKSE\\Plugins\\Wildfire.ini");
    if (!file.is_open()) {
        logger::error("Failed to open INI for writing");
        return;
    }

    file << "[General]\n";
    file << "bModActive=" << (ModActive ? "True" : "False") << "\n\n";

    file << "[Debug]\n";
    file << "bEnableTimeLogging=" << (EnableTimeLogging ? "True" : "False") << "\n";
    file << "bDebugMode=" << (DebugMode ? "True" : "False") << "\n";
    file << "bDrawImpacts=" << (DrawImpacts ? "True" : "False") << "\n";
    file << "bDrawLandscape=" << (DrawLandscape ? "True" : "False") << "\n";
    file << "bDrawBoundBoxes=" << (DrawBoundBoxes ? "True" : "False") << "\n";
    file << "bDrawHazards=" << (DrawHazards ? "True" : "False") << "\n";
    file << "bDrawHazardsMovement=" << (DrawHazardsMovement ? "True" : "False") << "\n";
    file << "bDrawDecals=" << (DrawDecals ? "True" : "False") << "\n";
    file << "bDrawWind=" << (DrawWind ? "True" : "False") << "\n";
    file << "iDebugDrawDuration=" << DebugDrawDuration << "\n\n";

    file << "[Timers]\n";
    file << "fGrassPeriodicUpdateTime=" << GrassPeriodicUpdateTime << "\n";
    file << "fHazardPeriodicUpdateTime=" << HazardPeriodicUpdateTime << "\n";
    file << "fRefPeriodicUpdateTime=" << RefPeriodicUpdateTime << "\n\n";

    file << "[Grass]\n";
    file << "fDefaultGrassIgnitionHeat=" << DefaultGrassIgnitionHeat << "\n";
    file << "fDefaultGrassFuel=" << DefaultGrassFuel << "\n";
    file << "fDefaultMaxGrassHeat=" << DefaultMaxGrassHeat << "\n";
    file << "fDefaultMinGrassHeat=" << DefaultMinGrassHeat << "\n";
    file << "iMinGrassColorValue=" << static_cast<int>(MinGrassColorValue) << "\n";
    file << "fGrassHeightDecrease=" << GrassHeightDecrease << "\n\n";

    file << "[Refs]\n";
    file << "fDefaultRefIgnitionHeat=" << DefaultRefIgnitionHeat << "\n";
    file << "fDefaultRefFuel=" << DefaultRefFuel << "\n";
    file << "fDefaultMaxRefHeat=" << DefaultMaxRefHeat << "\n";
    file << "fDefaultMinRefHeat=" << DefaultMinRefHeat << "\n";
    file << "fUnderwaterRatio=" << UnderwaterRatio << "\n";
    file << "fFuelRecoveryRate=" << FuelRecoveryRate << "\n\n";

    file << "[Hazards]\n";
    file << "iMaxHazardSpawn=" << MaxHazardSpawn << "\n";
    file << "iMaxSmokeHazards=" << MaxSmokeHazards << "\n";
    file << "bHazardAffectPCOnly=" << (HazardAffectPCOnly ? "True" : "False") << "\n\n";

    file << "[Decals]\n";
    file << "fMinDacalSize=" << MinDacalSize << "\n";
    file << "fMaxDacalSize=" << MaxDacalSize << "\n";
    file << "iDecalSpacing=" << DecalSpacing << "\n";
    file << "iDecalSamplingPoints=" << DecalSamplingPoints << "\n\n";

    file << "[FireSpreading]\n";
    file << "fHeatDistributionFactor=" << HeatDistributionFactor << "\n";
    file << "fMaxHeatDistribution=" << MaxHeatDistribution << "\n";
    file << "fFuelConsumptionRate=" << FuelConsumptionRate << "\n";
    file << "fFuelToHeatRate=" << FuelToHeatRate << "\n";
    file << "fSelfHeatLoss=" << SelfHeatLoss << "\n";
    file << "fSelfColdLoss=" << SelfColdLoss << "\n\n";

    file << "[DamageMultipliers]\n";
    file << "fFireDamageMultiplayer=" << FireDamageMultiplayer << "\n";
    file << "fShockDamageMultiplayer=" << ShockDamageMultiplayer << "\n";
    file << "fColdDamageMultiplayer=" << ColdDamageMultiplayer << "\n";
    file << "fWaterDamageMultiplayer=" << WaterDamageMultiplayer << "\n\n";

    file << "[Weather]\n";
    file << "fRainingFactor=" << RainingFactor << "\n";
    file << "fSnowingFactor=" << SnowingFactor << "\n";
    file << "fWindSpeedFactor=" << WindSpeedFactor << "\n";

    logger::info("Settings saved to INI");
}

void Settings::ResetIni() {
    // Debug
    EnableTimeLogging = true;
    DebugMode = false;
    DrawImpacts = true;
    DrawLandscape = true;
    DrawBoundBoxes = true;
    DrawHazards = true;
    DrawHazardsMovement = false;
    DrawDecals = true;
    DrawWind = true;
    DebugDrawDuration = 1000;

    // Timers
    GrassPeriodicUpdateTime = 1.0f;
    HazardPeriodicUpdateTime = 1.5f;
    RefPeriodicUpdateTime = 1.0f;

    // Grass
    DefaultGrassIgnitionHeat = 25.0f;
    DefaultGrassFuel = 25.0f;
    DefaultMaxGrassHeat = 100.0f;
    DefaultMinGrassHeat = -50.0f;
    MinGrassColorValue = 45;
    GrassHeightDecrease = 25.0f;

    // Refs
    DefaultRefIgnitionHeat = 50.0f;
    DefaultRefFuel = 50.0f;
    DefaultMaxRefHeat = 250.0f;
    DefaultMinRefHeat = -100.0f;
    UnderwaterRatio = 0.5f;
    FuelRecoveryRate = 1.0f;

    // Hazards
    MaxHazardSpawn = 400;
    MaxSmokeHazards = 100;
    HazardAffectPCOnly = true;

    // Decals
    MinDacalSize = 500.0f;
    MaxDacalSize = 650.0f;
    DecalSpacing = 5;
    DecalSamplingPoints = 3;

    // Fire spreading
    HeatDistributionFactor = 10.0f;
    MaxHeatDistribution = 5.0f;
    FuelConsumptionRate = 1.0f;
    FuelToHeatRate = 0.50f;
    SelfHeatLoss = 0.10f;
    SelfColdLoss = 0.50f;

    // Damage Multiplayers
    FireDamageMultiplayer = 1.0f;
    ShockDamageMultiplayer = 0.5f;
    ColdDamageMultiplayer = 1.5f;
    WaterDamageMultiplayer = 1.0f;

    // Wheather
    RainingFactor = 0.50f;
    SnowingFactor = 0.75f;
    WindSpeedFactor = 2.0f;
}

void Settings::LoadSettings() {

    LoadIni();

    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\NeutralSources", neutralSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\FireSources", fireSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\ShockSources", shockSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\ColdSources", coldSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\WaterSources", waterSources);

    grassConfig = LoadGrassConfigs("Data\\SKSE\\Plugins\\Wildfire\\Grass");
    refsConfig = LoadRefConfigs("Data\\SKSE\\Plugins\\Wildfire\\Refs");

    excludedTrees = LoadFormSet("Data\\SKSE\\Plugins\\Wildfire\\ExcludedTrees");

    logger::info("Loaded {} fire source patterns", fireSources.size());
    logger::info("Loaded {} shock source patterns", shockSources.size());
    logger::info("Loaded {} cold source patterns", coldSources.size());
    logger::info("Loaded {} water source patterns", waterSources.size());

    logger::info("Loaded {} grass configs", grassConfig.size());
    logger::info("Loaded {} ref configs", refsConfig.size());
    logger::info("Loaded {} excluded trees", excludedTrees.size());
}