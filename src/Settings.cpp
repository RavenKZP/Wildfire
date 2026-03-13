#include "Settings.h"
#include "Utils.h"

#include <nlohmann/json.hpp>

static ElementType ParseElementType(const std::string& str) {
    std::string s = Utils::ToLower(str);

    if (s == "fire") return ElementType::Fire;
    if (s == "shock") return ElementType::Shock;
    if (s == "cold") return ElementType::Cold;
    if (s == "water") return ElementType::Water;

    return ElementType::Neutral;
}

//ToDo: Add Support ESL
static RE::TESForm* GetFormFromString(const std::string& formString) {
    auto pos = formString.find('~');
    if (pos == std::string::npos) return nullptr;

    std::string idStr = formString.substr(0, pos);
    std::string plugin = formString.substr(pos + 1);

    uint32_t formID = std::stoul(idStr, nullptr, 16);

    auto handler = RE::TESDataHandler::GetSingleton();
    if (!handler) return nullptr;

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

// Load all pattern files in folder
static void LoadAllPatterns(const std::filesystem::path& folder, std::vector<std::string>& outPatterns) {
    if (!std::filesystem::exists(folder)) return;
    for (auto& entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file()) continue;
        LoadPatternsFromFile(entry.path(), outPatterns);
    }
}

template <class T>
T ParseBurnConfig(const nlohmann::json& j) {
    T cfg{};

    if (j.contains("canBurn")) cfg.canBurn = j["canBurn"];

    if (j.contains("fuel")) cfg.fuel = j["fuel"];

    if (j.contains("ignitionHeat")) cfg.ignitionHeat = j["ignitionHeat"];

    //if (j.contains("type")) cfg.type = j["type"];

    return cfg;
}

template <class T>
void LoadConfigFile(const std::filesystem::path& path, std::unordered_map<RE::FormID, T>& configs) {
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

        configs[form->GetFormID()] = ParseBurnConfig<T>(entry);
    }
}

template <class T>
std::unordered_map<RE::FormID, T> LoadConfigs(const std::filesystem::path& dir) {
    std::unordered_map<RE::FormID, T> configs;

    if (!std::filesystem::exists(dir)) return configs;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".json") {
            LoadConfigFile<T>(entry.path(), configs);
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

void Settings::LoadSettings() {
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\StrFireSources", fireSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\StrShockSources", shockSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\StrColdSources", coldSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\StrWaterSources", waterSources);

    grassConfig = LoadConfigs<GrassConfig>("Data\\SKSE\\Plugins\\Wildfire\\Grass");
    refsConfig = LoadConfigs<RefConfig>("Data\\SKSE\\Plugins\\Wildfire\\Refs");

    sourceForms = LoadSourceConfigs("Data\\SKSE\\Plugins\\Wildfire\\SourceForms");
    excludedTrees = LoadFormSet("Data\\SKSE\\Plugins\\Wildfire\\ExcludedTrees");

    logger::info("Loaded {} fire source patterns", fireSources.size());
    logger::info("Loaded {} shock source patterns", shockSources.size());
    logger::info("Loaded {} cold source patterns", coldSources.size());
    logger::info("Loaded {} water source patterns", waterSources.size());

    logger::info("Loaded {} grass configs", grassConfig.size());
    logger::info("Loaded {} ref configs", refsConfig.size());
    logger::info("Loaded {} source forms", sourceForms.size());
    logger::info("Loaded {} excluded trees", excludedTrees.size());
}