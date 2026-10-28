#include "Wildfire/DecalUpdater.h"

#include "Settings.h"
#include "Types.h"


#ifndef NDEBUG
#include "CLibUtilsQTR/DrawDebug.hpp"
#endif

#include <array>
#include <unordered_set>
#include <random>

// Credits goes to KernalsEgg:
// https://github.com/KernalsEgg/SKSE64Plugins/tree/9060187900d3df632d010127ac612fafc6775e5f
// and his awesome SKSE plugins with source
// Especially his Trails https://github.com/KernalsEgg/SKSE64Plugins/tree/9060187900d3df632d010127ac612fafc6775e5f/Trails
// Code below is mostly based on his work and his RE functions

template <class T>
    requires(std::is_enum_v<T>)
constexpr std::underlying_type_t<T> ToUnderlying(T enumeration) noexcept {
    return static_cast<std::underlying_type_t<T>>(enumeration);
}

static std::int32_t GetSystemGroupFromFilterInformation(RE::CFilter filterInformation) noexcept {
    const auto u = filterInformation.filter;
    return static_cast<std::int32_t>(u >> 16);
}

static void* GethkpCollidableOwner(RE::hkpCollidable* a_this) {
    return const_cast<char*>(reinterpret_cast<const char*>(a_this) + a_this->ownerOffset);
}

static RE::NiAVObject* GethkpCollidable3D(const RE::hkpCollidable* a_this) {
    auto* owner = a_this->GetOwner<RE::hkpWorldObject>();

    if (!owner) {
        return nullptr;
    }

    RE::bhkCollisionObject* property{nullptr};

    if (auto prop2 = owner->GetProperty(2)) {
        property = static_cast<RE::bhkCollisionObject*>(prop2->value.GetPointer());
    }

    if (property) {
        return property->sceneObject;
    }

    auto* userData = reinterpret_cast<RE::bhkWorldObject*>(owner->userData);

    if (userData) {
        if (auto prop1 = owner->GetProperty(1)) {
            return static_cast<RE::NiAVObject*>(prop1->value.GetPointer());
        }
    }

    return nullptr;
}

enum class SystemGroup : std::uint16_t { kTerrain = 1 };

DecalUpdater::DecalUpdater() {
    defaultImpactDataSet = RE::TESForm::LookupByEditorID<RE::BGSImpactDataSet>("Wildfire_FireImpactDataSet");

    if (defaultImpactDataSet) {
        logger::info("[DecalUpdater] Fire Impact Data Set found and loaded.");
    } else {
        Settings::GetSingleton()->ModActive = false;
        logger::error("[DecalUpdater] Fire Impact Data Set not found. Please make sure Wildfire.esp is active");
        RE::DebugMessageBox("Wildfire Error: Fire Impact Data Set not found. Please make sure Wildfire.esp is active");
    }
}

bool DecalUpdater::SpawnBurnDecalAtPosition(const RE::NiPoint3& worldPosition) {
    static auto* set = Settings::GetSingleton();
    auto start = std::chrono::high_resolution_clock::now();

    if (set->EnableTimeLogging) {
        logger::info("[Main][DecalUpdater] Start to Spawn Decal");
    }

    auto* parentCell = RE::TES::GetSingleton()->GetCell(worldPosition);

    if (!parentCell) {
        return false;
    }

    auto* havokWorld = parentCell->GetbhkWorld();

    if (!havokWorld) {
        return false;
    }

    auto* impactDataSet = defaultImpactDataSet;

    if (!impactDataSet) {
        return false;
    }

    RE::NiPoint3 origin = worldPosition;
    RE::NiPoint3 ray = {0.0f, 0.0f, -512.0f};
    origin.z += 64.0f;

    RE::bhkPickData pickData;
    RE::hkpClosestRayHitCollector closestRayHitCollector;

    pickData.closestRayHitCollector = std::addressof(closestRayHitCollector);

    auto havokWorldScale = RE::bhkWorld::GetWorldScale();

    pickData.rayInput.from.quad =
        ::_mm_setr_ps(origin.x * havokWorldScale, origin.y * havokWorldScale, origin.z * havokWorldScale, 0.0F);

    pickData.ray.quad = ::_mm_setr_ps(ray.x * havokWorldScale, ray.y * havokWorldScale, ray.z * havokWorldScale, 0.0F);

    if (!havokWorld->PickObject(pickData)) {
        return false;
    }

    const auto& rayHit = closestRayHitCollector.rayHit;
    const auto* rootCollidable = rayHit.rootCollidable;

    if (!rootCollidable) {
#ifndef NDEBUG
        if (set->DebugMode && set->DrawDecals) {
            static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
            RE::NiColorA white{0.5f, 0.5f, 0.5f, 1.0f};  // Gray
            const float duration = set->DebugDrawDuration * 10.0f;
            const float thickness = 2.0f;
            DebugAPI->DrawLineForMS(origin, origin + ray, duration, white, thickness);
        }
#endif
        return false;
    }

    auto position = origin + (ray * rayHit.hitFraction);

    RE::NiPoint3 normal = {rayHit.normal.quad.m128_f32[0], rayHit.normal.quad.m128_f32[1],
                           rayHit.normal.quad.m128_f32[2]};

    normal.Unitize();

#ifndef NDEBUG
    // Debug: draw raycast line (origin -> hit position) in white when enabled
    if (set->DebugMode && set->DrawDecals) {
        static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
        RE::NiColorA white{1.0F, 1.0F, 1.0F, 1.0F};  // White
        const float duration = set->DebugDrawDuration * 10.0f;
        const float thickness = 2.0f;
        DebugAPI->DrawLineForMS(origin, position, duration, white, thickness);
    }
#endif

    RE::MATERIAL_ID materialID{0};
    RE::NiAVObject* target3D{nullptr};

    auto filterInfo = rootCollidable->broadPhaseHandle.collisionFilterInfo;
    auto terrain = GetSystemGroupFromFilterInformation(filterInfo) == ToUnderlying(SystemGroup::kTerrain);

    if (terrain) {
        materialID = RE::TES::GetSingleton()->GetLandMaterialType(position);

        if (rootCollidable->broadPhaseHandle.type == ToUnderlying(RE::hkpWorldObject::BroadPhaseType::kEntity)) {
            auto* owner = rootCollidable->GetOwner<RE::hkpWorldObject>();

            if (owner) {
                auto optProp = owner->GetProperty(1);

                if (optProp) {
                    auto* property = static_cast<RE::NiAVObject*>(optProp->value.GetPointer());

                    if (property) {
                        auto* parentNode = property->parent;

                        if (parentNode) {
                            target3D = parentNode;
                        }
                    }
                }
            }
        }
    } else {
        const auto* shape = rootCollidable->shape;

        if (shape) {
            auto* userData = reinterpret_cast<RE::bhkShape*>(shape->userData);

            if (userData) {
                auto shapeKey = RE::HK_INVALID_SHAPE_KEY;
                const auto& shapeKeys = rayHit.shapeKeys;

                for (std::int32_t shapeKeyIndex = 0; shapeKeyIndex < RE::hkpShapeRayCastOutput::kMaxHierarchyDepth;
                     ++shapeKeyIndex) {
                    if (shapeKeys[shapeKeyIndex] == RE::HK_INVALID_SHAPE_KEY) {
                        break;
                    }

                    shapeKey = shapeKeys[shapeKeyIndex];
                }

                materialID = userData->GetMaterialID(shapeKey);
            }
        }
        target3D = GethkpCollidable3D(rootCollidable);
    }
    if (!target3D) {
        return false;
    }

    auto* materialType = RE::BGSMaterialType::GetMaterialType(materialID);
    if (!materialType) {
        return false;
    }

    auto scatterTable = impactDataSet->impactMap.find(materialType);
    if (scatterTable == impactDataSet->impactMap.end()) {
        return false;
    }

    auto* impactData = scatterTable->second;
    if (!impactData) {
        return false;
    }

    auto* primaryTextureSet = impactData->decalTextureSet;
    if (!primaryTextureSet) {
        return false;
    }

    auto* target = target3D->GetUserData();
    if (!terrain && !target) {
        return false;
    }

    RE::BGSDecalManager::CreationData creationData;
    creationData.position = position;
    creationData.normal = normal;
    creationData.target3D = RE::NiPointer<RE::NiAVObject>(target3D);
    creationData.primaryTextureSet = primaryTextureSet;
    creationData.secondaryTextureSet = impactData->decalTextureSet2;

    const auto& decalData = impactData->dData;

    // this width height is something wrong, seems not matching CK data or decalData is somehow diffrent than creationData
    std::mt19937 randomNumberGenerator(std::random_device{}());
    std::uniform_real_distribution widthDistribution(set->MinDacalSize, set->MaxDacalSize);
    float scale = widthDistribution(randomNumberGenerator);
    creationData.width = scale;
    creationData.height = scale;
    creationData.depth = decalData.data.depth * 10.0f; // So they don't get cut on slopes

    std::uniform_real_distribution rotationDistribution(-M_PI, M_PI);
    const float rotation = rotationDistribution(randomNumberGenerator);

    creationData.rotation.EulerAnglesToAxesZXY(0.0F, 0.0F, rotation);
    if (!target) {
        creationData.parentCell = parentCell;
    }

    creationData.parallaxScale = decalData.data.parallaxScale;
    creationData.shininess = decalData.data.shininess;
    creationData.angleThreshold = impactData->data.angleThreshold;
    creationData.placementRadius = impactData->data.placementRadius;

    creationData.color = RE::NiColor(static_cast<float>(decalData.data.color.red) / 255.0F,
                                     static_cast<float>(decalData.data.color.green) / 255.0F,
                                     static_cast<float>(decalData.data.color.blue) / 255.0F);

    auto decalDataFlags = decalData.data.flags;

    // if (decalDataFlags.all(RE::DECAL_DATA_DATA::Flag::kNoSubtextures)) {
    //     creationData.subTextureIndex = -1;
    // } else {
    std::uniform_int_distribution subtextureIndexDistribution(0, 3);

    creationData.subTextureIndex = static_cast<std::int8_t>(subtextureIndexDistribution(randomNumberGenerator));
    //}

    // creationData.parallax = decalDataFlags.all(RE::DECAL_DATA_DATA::Flag::kParallax);
    // creationData.alphaTesting = decalDataFlags.all(RE::DECAL_DATA_DATA::Flag::kAlphaTesting);
    // creationData.alphaBlending = decalDataFlags.all(RE::DECAL_DATA_DATA::Flag::kAlphaBlending);

    creationData.parallaxPasses = decalData.data.parallaxPasses;

    AddDecal(creationData, true, nullptr);

    // --- Extension: check four corners and spawn additional decals
    // to avoid decal being "cut" at cell/quadrant/mesh boundaries.
    {
        // track which rootCollidable we've already processed (prevents creating multiple decals
        // for the same object when several corners hit it)
        std::unordered_set<const RE::hkpCollidable*> processedRoots;
        processedRoots.reserve(4);
        processedRoots.insert(rootCollidable);

        const float offset = 0.5f * scale;
        const float halfOffset = 0.5f * offset;

        const int samplingPoints = std::max(4, set->DecalSamplingPoints * 4);

        std::vector<std::pair<float, float>> cornerOffsets;
        cornerOffsets.reserve(samplingPoints);

        // Always add the 4 corners
        cornerOffsets = {{offset, offset}, {offset, -offset}, {-offset, offset}, {-offset, -offset}};

        const int additionalPoints = (samplingPoints - 4) / 4;

        for (int i = 1; i <= additionalPoints; ++i) {
            const bool edge = (i % 2 == 1);
            const int pointsPerSide = (i + 1) / 2;

            const float step = offset / static_cast<float>(pointsPerSide);

            if (edge) {
                // Add points between the corners on each edge.
                for (int j = 1; j <= pointsPerSide; ++j) {
                    const float p =
                        -offset + 2.0f * offset * (static_cast<float>(j) / static_cast<float>(pointsPerSide + 1));

                    cornerOffsets.emplace_back(p, offset);
                    cornerOffsets.emplace_back(p, -offset);
                    cornerOffsets.emplace_back(offset, p);
                    cornerOffsets.emplace_back(-offset, p);
                }
            } else {
                // Add points along the diagonals.
                for (int j = 1; j <= pointsPerSide; ++j) {
                    const float p =
                        -offset + 2.0f * offset * (static_cast<float>(j) / static_cast<float>(pointsPerSide + 1));

                    cornerOffsets.emplace_back(p, p);
                    cornerOffsets.emplace_back(p, -p);
                    cornerOffsets.emplace_back(-p, p);
                    cornerOffsets.emplace_back(-p, -p);
                }
            }
        }

#ifndef NDEBUG
        if (set->DebugMode && set->DrawDecals) {
            static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
            RE::NiColorA colour{1, 1, 0, 1};  // Yellow
            const float duration = set->DebugDrawDuration * 10.0f;
            const float thickness = 2.0f;

            std::array<RE::NiPoint3, 4> cornerPositions;
            for (std::size_t i = 0; i < 4; ++i) {
                cornerPositions[i] = position;
                cornerPositions[i].x += cornerOffsets[i].first;
                cornerPositions[i].y += cornerOffsets[i].second;
            }

            // Edges
            DebugAPI->DrawLineForMS(cornerPositions[0], cornerPositions[1], duration, colour, thickness);
            DebugAPI->DrawLineForMS(cornerPositions[1], cornerPositions[3], duration, colour, thickness);
            DebugAPI->DrawLineForMS(cornerPositions[3], cornerPositions[2], duration, colour, thickness);
            DebugAPI->DrawLineForMS(cornerPositions[2], cornerPositions[0], duration, colour, thickness);

            // Diagonal
            DebugAPI->DrawLineForMS(cornerPositions[0], cornerPositions[3], duration, colour, thickness);
            DebugAPI->DrawLineForMS(cornerPositions[1], cornerPositions[2], duration, colour, thickness);
        }
#endif

        for (const auto& [dx, dy] : cornerOffsets) {
            RE::NiPoint3 cornerOrigin = position;
            cornerOrigin.x += dx;
            cornerOrigin.y += dy;
            cornerOrigin.z += 128.0f;

            RE::bhkPickData cornerPickData;
            RE::hkpClosestRayHitCollector cornerCollector;
            cornerPickData.closestRayHitCollector = std::addressof(cornerCollector);

            const auto havokWorldScaleLocal = RE::bhkWorld::GetWorldScale();
            cornerPickData.rayInput.from.quad = ::_mm_setr_ps(cornerOrigin.x * havokWorldScaleLocal,
                                                             cornerOrigin.y * havokWorldScaleLocal,
                                                             cornerOrigin.z * havokWorldScaleLocal,
                                                             0.0F);

            cornerPickData.ray.quad = ::_mm_setr_ps(ray.x * havokWorldScaleLocal,
                                                    ray.y * havokWorldScaleLocal,
                                                    ray.z * havokWorldScaleLocal,
                                                    0.0F);

            if (!havokWorld->PickObject(cornerPickData)) {
#ifndef NDEBUG
                if (set->DebugMode && set->DrawDecals) {
                    static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
                    RE::NiColorA yellow{1 ,1, 0, 1};
                    const float duration = set->DebugDrawDuration * 10.0f;
                    const float thickness = 2.0f;
                    DebugAPI->DrawLineForMS(cornerOrigin, cornerOrigin + ray, duration, yellow, thickness);
                }
#endif
                continue;
            }

            const auto& cornerHit = cornerCollector.rayHit;
            const auto* cornerRoot = cornerHit.rootCollidable;
            if (!cornerRoot) {
#ifndef NDEBUG
                if (set->DebugMode && set->DrawDecals) {
                    static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
                    RE::NiColorA yellow{1, 1, 0, 1};
                    const float duration = set->DebugDrawDuration * 10.0f;
                    const float thickness = 2.0f;
                    DebugAPI->DrawLineForMS(cornerOrigin, cornerOrigin + ray, duration, yellow, thickness);
                }
#endif
                continue;
            }

            RE::NiPoint3 cornerPosition = cornerOrigin + (ray * cornerHit.hitFraction);

            // if we've already processed this rootCollidable (original or another corner), skip it
            if (processedRoots.find(cornerRoot) != processedRoots.end()) {
                continue;
            }

            processedRoots.insert(cornerRoot);

            RE::NiPoint3 cornerNormal = {cornerHit.normal.quad.m128_f32[0],
                                         cornerHit.normal.quad.m128_f32[1],
                                         cornerHit.normal.quad.m128_f32[2]};
            cornerNormal.Unitize();

#ifndef NDEBUG
            // Debug: draw raycast line (origin -> hit position) in white when enabled
            if (set->DebugMode && set->DrawDecals) {
                static auto* DebugAPI = DebugAPI_IMPL::DebugAPI::GetSingleton();
                RE::NiColorA white{1, 1, 1, 1};  // White
                const float duration = set->DebugDrawDuration * 10.0f;
                const float thickness = 2.0f;
                DebugAPI->DrawLineForMS(cornerOrigin, cornerPosition, duration, white, thickness);
            }
#endif

            RE::NiAVObject* cornerTarget3D{nullptr};
            auto cornerFilterInfo = cornerRoot->broadPhaseHandle.collisionFilterInfo;
            auto cornerTerrain = GetSystemGroupFromFilterInformation(cornerFilterInfo) == ToUnderlying(SystemGroup::kTerrain);

            if (cornerTerrain) {
                if (cornerRoot->broadPhaseHandle.type == ToUnderlying(RE::hkpWorldObject::BroadPhaseType::kEntity)) {
                    auto* owner = cornerRoot->GetOwner<RE::hkpWorldObject>();
                    if (owner) {
                        auto optProp = owner->GetProperty(1);
                        if (optProp) {
                            auto* property = static_cast<RE::NiAVObject*>(optProp->value.GetPointer());
                            if (property && property->parent) {
                                cornerTarget3D = property->parent;
                            }
                        }
                    }
                }
            } else {
                cornerTarget3D = GethkpCollidable3D(cornerRoot);
            }
            if (!cornerTarget3D) {
                continue;
            }

            // Create a copy of creationData and update normal/target/parentCell
            RE::BGSDecalManager::CreationData cornerCreation = creationData; // copy all params
            //cornerCreation.position = cornerPosition; // spawn in the same place to match other
            cornerCreation.normal = cornerNormal;
            cornerCreation.target3D = RE::NiPointer<RE::NiAVObject>(cornerTarget3D);

            if (!cornerTarget3D) {
                cornerCreation.parentCell = RE::TES::GetSingleton()->GetCell(cornerPosition);
            } else {
                cornerCreation.parentCell = nullptr;
            }

            // Add additional decal with the same visual parameters
            AddDecal(cornerCreation, true, nullptr);
        }

        if (set->EnableTimeLogging) {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;
            logger::info("[Main][DecalUpdater] Spawned decal on {} meshes in {} ms", processedRoots.size(),
                         elapsed.count());
        }
    }

    return true;
}

void DecalUpdater::AddDecal(const RE::BGSDecalManager::CreationData& creationData, bool forceDecal,
                            RE::BGSDecalGroup* decalGroup) {
    auto decalManager = RE::BGSDecalManager::GetSingleton();

    decalManager->unknown14 = true;

    if (creationData.target3D) {
        auto* target3DNode = creationData.target3D->AsNode();

        if (target3DNode) {
            target3DNode->AddDecal(RE::BGSDecalManager::AdditionData{
                .creationData = std::addressof(creationData),
                .forceDecal = forceDecal,
                .unknown9 = creationData.unknownC0 != 0,
                .decalCount = decalManager->decalCount,
                .skinDecalCount = decalManager->skinDecalCount,
                .skinDecalCountCurrentFrame = decalManager->skinDecalCountCurrentFrame,
                .decalCountCurrentFrame = decalManager->decalCountCurrentFrame,
                .unknown1C = 0,
                .decalGroup = decalGroup});
        }
    }
}
