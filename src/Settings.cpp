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
        config.EditorID = clib_util::editorID::get_editorID(form);
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

void Settings::LoadSettings() {
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\StrFireSources", fireSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\StrShockSources", shockSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\StrColdSources", coldSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\StrWaterSources", waterSources);

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