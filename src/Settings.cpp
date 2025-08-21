#include "Settings.h"
#include "Utils.h"

inline void from_json(const nlohmann::json& j, GrassFireConfig& cfg) {
    j.at("name").get_to(cfg.name);
    j.at("canBurn").get_to(cfg.canBurn);
    j.at("fuel").get_to(cfg.fuel);
    j.at("minBurnHeat").get_to(cfg.minBurnHeat);
}

std::vector<GrassFireConfig> LoadGrassConfigFromFile(const std::filesystem::path& path) {
    std::vector<GrassFireConfig> result;
    std::ifstream file(path);
    if (!file.is_open()) return result;

    nlohmann::json j;
    file >> j;
    if (!j.is_array()) return result;

    for (const auto& entry : j) {
        result.push_back(entry.get<GrassFireConfig>());
    }
    return result;
}
std::vector<GrassFireConfig> LoadAllGrassConfigs(const std::filesystem::path& dir) {
    std::vector<GrassFireConfig> allConfigs;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".json") {
            auto configs = LoadGrassConfigFromFile(entry.path());
            allConfigs.insert(allConfigs.end(), configs.begin(), configs.end());
        }
    }
    return allConfigs;
}

// Load substring patterns (one per line, '#' prefix = comment)
void LoadPatternsFromFile(const std::filesystem::path& path, std::vector<std::string>& outPatterns) {
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
void LoadAllPatterns(const std::filesystem::path& folder, std::vector<std::string>& outPatterns) {
    if (!std::filesystem::exists(folder)) return;
    for (auto& entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file()) continue;
        LoadPatternsFromFile(entry.path(), outPatterns);
    }
}

void Settings::LoadSettings() {
    grassConfigs = LoadAllGrassConfigs("Data\\SKSE\\Plugins\\Wildfire\\Grass");
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\FireSources", fireSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\ColdSources", coldSources);
    LoadAllPatterns("Data\\SKSE\\Plugins\\Wildfire\\WaterSources", waterSources);

    logger::info("Loaded {} grass configs", grassConfigs.size());
    logger::info("Loaded {} fire source patterns", fireSources.size());
    logger::info("Loaded {} cold source patterns", coldSources.size());
    logger::info("Loaded {} water source patterns", waterSources.size());
}