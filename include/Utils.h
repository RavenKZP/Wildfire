#pragma once

#include "Types.h"

#include "REX/REX.h"

#include <shared_mutex>

namespace Utils {
    float RandomFloat(float min, float max);

    std::pair<int, int> GetCellCoords(RE::TESObjectCELL* cell);

    RE::TESObjectCELL* GetCellByCoords(int cellX, int cellY);
    RE::TESObjectCELL* GetCellByCoords(const CellAsCoords& coords);

    RE::NiPoint3 GetPosition(const RE::TESObjectREFR* obj);
    BoundaryBox GetBoundingBox(const RE::TESObjectREFR* a_obj);

    bool IsSupportedForm(RE::TESObjectREFR* ref);
    bool IsPapyrusSupported(RE::TESObjectREFR* ref);
    bool IsOIFSupported(RE::TESObjectREFR* ref);

    RefType GetGeometryType(RE::TESObjectREFR* ref);

    std::string ToLower(std::string s);

    ElementType GetProjectileType(RE::Projectile* proj);
    ElementType GetExplosionType(RE::Explosion* exp);

    std::string ElementTypeToStr(ElementType type);

    float GetDamageFromProjectile(RE::Projectile* proj);

    RE::BSFixedString GetModelPath(RE::TESBoundObject* base);

    class DetectionCache : public REX::Singleton<DetectionCache> {
    public:
        std::optional<ElementType> Get(RE::FormID id) {
            std::shared_lock<std::shared_mutex> readLock(cacheMutex_);
            auto it = cache_.find(id);
            if (it != cache_.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        void Add(RE::FormID id, ElementType type) {
            std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
            cache_[id] = type;
        }

    private:
        std::unordered_map<RE::FormID, ElementType> cache_;
        mutable std::shared_mutex cacheMutex_;
    };

    class ProjectileUpdate : public REX::Singleton<ProjectileUpdate> {
    public:
        float Get(RE::FormID id) {
            std::shared_lock<std::shared_mutex> readLock(cacheMutex_);
            auto it = cache_.find(id);
            if (it != cache_.end()) {
                return it->second;
            }
            return -1.0f;
        }

        void Add(RE::FormID id, float livingTime) {
            std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
            cache_[id] = livingTime;
        }

        void Update(RE::FormID id, float livingTime) {
            std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
            cache_[id] = livingTime;
        }

        void Remove(RE::FormID id) {
            std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
            cache_.erase(id);
        }

    private:
        std::unordered_map<RE::FormID, float> cache_;
        mutable std::shared_mutex cacheMutex_;
    };
}