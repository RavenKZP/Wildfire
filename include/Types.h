#pragma once

struct GrassFireConfig {
    std::string name;
    bool canBurn;
    uint8_t fuel;
    uint8_t minBurnHeat;
};

enum ProjectileType {
    Unknown,
    Fire,
    Cold,
    Water,
};

struct GrassData {
    bool canBurn = false;    // Whether the grass can burn
    float extraFuel = 0.0f;  // Extra fuel provided by this grass
    float extraHeat = 0.0f;  // Extra heat provided by this grass
};

struct WindData {
    uint8_t speed;
    uint8_t direction;
};

struct FireCellState {
    uint8_t originalColors[4][289][3];  // Original colors for each vertex
    float heat[4][289];
    float minBurnHeat[4][289];
    float fuel[4][289];
    bool isBurning[4][289];
    bool canBurn[4][289];
    bool isCharred[4][289];
    bool altered;

    FireCellState(RE::TESObjectCELL* cell);
};

struct FireVertex {
    RE::TESObjectCELL* cell;  // Pointer to the cell containing this vertex
    int quadrant;             // 0-3
    int vertex;               // 0-288
};

struct WeightedNeighbour {
    FireVertex vertex;
    float weight;
};

struct HazardGridCoord {
    int x, y;
    bool operator==(const HazardGridCoord& other) const noexcept { return x == other.x && y == other.y; }
};

namespace std {
    template <>
    struct hash<HazardGridCoord> {
        std::size_t operator()(const HazardGridCoord& point) const noexcept {
            std::size_t h1 = std::hash<int>()(point.x);
            std::size_t h2 = std::hash<int>()(point.y);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}