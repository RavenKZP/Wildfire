#pragma once

#define CELL_QUAD_COUNT 4
#define VERTS_PER_QUAD 289  // 17x17 grid
#define VERTS_PER_ROW 34    // 2 quads per row, 17 vertices each
#define VERTS_PER_QUAD_ROW 17
#define QUADS_PER_ROW 2
#define VERTS_STEP 128  // world units between vertices
#define CELL_SIZE 4096  // world units
#define QUAD_SIZE 2048  // world units
#define RGB_CHANNELS 3 // Red, Green, Blue
#define TEXTURES_PER_QUAD 6 // Vanilla limitation, quad can have up to 6 textures at once

#define M_PI 3.14159265358979323846f

#include <array>
#include <cstring>

using BoundaryBox = std::array<RE::NiPoint3, 8>;

enum ElementType {
    Unknown = -1,
    Neutral = 0,
    Fire,
    Shock,
    Cold,
    Water,
};

enum RefType {
    Static,                  // Default behavior. Apply Charred FX
    Tree,                    // Remove leafs and branches, but keep trunk. Applay Charred FX, remove wind animation
    Bush,                    // Replace with predefined base object
    Melt,                    // Snow or Ice, clothing. Applay melting Transform
    Disintegrate,            // Banners, clothing, Ivy, moss. Applay disintegrate FX
    FloraCoveredUnburnable,  // Ivy covered Stones, remove leafs and branches (like Trees), but don't apply Charred FX
    BaseObjSwap,             // Replace with configured base object
};

enum HazardType {
    SmallFireHazard = 1,
    MediumFireHazard = 2,
    BigFireHazard = 3,
    LargeFireHazard = 4
};

struct GrassEvent {
    RE::NiPoint3 pos;
    float radius;
    float damage;
    ElementType type;
};

struct RefEvent {
    RE::ObjectRefHandle refHandle;
    float damage;
    ElementType type;
};

struct GrassConfig {
    bool canBurn;
    float fuel;
    float ignitionHeat;
    std::string EditorID;
};

struct RefConfig {
    bool canBurn;
    float fuel;
    float ignitionHeat;
    RefType type;
    RE::TESBoundObject* baseObjectSwap = nullptr;
    std::string EditorID;
};

struct WindData {
    float speed;
    float direction;
};

struct WeatherData {
    bool isRaining;
    bool isSnowing;
    WindData windData;
};

struct CellAsCoords {
    int cellX;
    int cellY;
    bool operator==(const CellAsCoords& other) const noexcept { return cellX == other.cellX && cellY == other.cellY; }
};

struct CellState {
    bool valid;
    CellAsCoords cellAsCoordinates;                                         // Cell coordinates (cellX, cellY)

    uint8_t originalColors[CELL_QUAD_COUNT][VERTS_PER_QUAD][RGB_CHANNELS];  // Original colors for each vertex
    uint8_t newColors[CELL_QUAD_COUNT][VERTS_PER_QUAD][RGB_CHANNELS];
    float originalHeights[CELL_QUAD_COUNT][VERTS_PER_QUAD];                 // store original heights (copied once)
    float newHeights[CELL_QUAD_COUNT][VERTS_PER_QUAD];                         // current heights (modified by burn)

    float heat[CELL_QUAD_COUNT][VERTS_PER_QUAD];
    float usedHeat[CELL_QUAD_COUNT][VERTS_PER_QUAD];
    float ignitionHeat[CELL_QUAD_COUNT][VERTS_PER_QUAD];
    float fuel[CELL_QUAD_COUNT][VERTS_PER_QUAD];

    // ToDo: Make them Flags to take less space
    bool canBurn[CELL_QUAD_COUNT][VERTS_PER_QUAD];
    bool isBurning[CELL_QUAD_COUNT][VERTS_PER_QUAD];
    bool isCharred[CELL_QUAD_COUNT][VERTS_PER_QUAD];
    bool isUnderwater[CELL_QUAD_COUNT][VERTS_PER_QUAD];  // Just for debugging canBurn covers underwater case
    bool altered[CELL_QUAD_COUNT][VERTS_PER_QUAD];

    // Constructor to initialize the CellState based on the cell's current state
    CellState(RE::TESObjectCELL* cell);
};

struct RefState {
    // Base data to dont use RE::TESObjectREFR* in threads
    RE::NiPoint3 position;
    RE::NiPoint3 angle;
    BoundaryBox BBox;

    float heat;
    float ignitionHeat;
    float initialFuel;
    float fuel;

    // ToDo: Make them Flags to take less space
    bool canBurn;
    bool isBurning;
    bool isCharred;
    bool isUnderwater;
    bool isFrozen;  // Let ig GO, Let it GOOOOOOOOO
    bool halfEvent;

    RefType type;
    RE::TESBoundObject* baseObjectSwap;
    RE::TESBoundObject* originalBaseObj;

    RefState(RE::TESObjectREFR* ref);
};

class FireVertex {
public:
    CellAsCoords cell;
    int quadrant;             // 0-3
    int vertex;               // 0-288
    RE::NiPoint3 worldsPos;   // World position of the vertex calculated in consrtuctor

    FireVertex(CellAsCoords cell, int quadrant, int vertex);
};

struct WeightedNeighbour {
    FireVertex vertex;
    float weight;
};

struct SpawnHazardData {
    RE::NiPoint3 position;
    HazardType size;  // 1=small_hazards, 2=medium, 3=large
    RE::ObjectRefHandle refHandle;  // Optional ref to be attached to
    bool highObject{false};
};

struct ObjectRefHandleHash {
    std::size_t operator()(const RE::ObjectRefHandle& h) const noexcept {
        return std::hash<std::uint32_t>()(h.native_handle());
    }
};

struct ObjectRefHandleEqual {
    bool operator()(const RE::ObjectRefHandle& a, const RE::ObjectRefHandle& b) const noexcept {
        return a.native_handle() == b.native_handle();
    }
};

struct TransformingData {
    RE::ObjectRefHandle refHandle;
    float Progress = 1.0f; // Going down to 0.0f
    RE::TESBoundObject* baseObjectSwap{nullptr};
};

using GrassEventType = std::vector<GrassEvent>;
using RefEventType = std::vector<RefEvent>;
using CellStateMapType = std::unordered_map<RE::TESObjectCELL*, CellState>;
using RefStateMapType = std::unordered_map<RE::ObjectRefHandle, RefState, ObjectRefHandleHash, ObjectRefHandleEqual>;
