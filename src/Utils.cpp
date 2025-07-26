#include "Utils.h"	
#include "DrawDebug.h"
#include "WildfireMgr.h"

#include <unordered_set>

namespace Utils {

    float GetDamageFromProjectile(RE::Projectile* proj) {
        if (!proj) {
            return 0.0f;
        }

        const auto& data = proj->GetProjectileRuntimeData();

        RE::Actor* actor = nullptr;
        if (data.shooter) {
            actor = data.shooter.get().get()->As<RE::Actor>();
        }

        if (data.spell) {
            if (actor) {
                return data.spell->CalculateMagickaCost(actor);
            } else {
                return data.spell->CalculateMagickaCost(nullptr);
            }
        }

        if (data.ammoSource) {
            return static_cast<float>(data.ammoSource->data.damage);
        }

        if (data.weaponSource) {
            return static_cast<float>(data.weaponSource->attackDamage);
        }

        return 0.0f;
    }

    RE::NiPoint3 GetWorldPosition(const FireVertex& vertex) {
        if (!vertex.cell) return RE::NiPoint3{0.0f, 0.0f, 0.0f};

        // Get cell grid coordinates
        auto coords = vertex.cell->GetCoordinates();
        int cellX = coords->cellX;
        int cellY = coords->cellY;

        // Cell world origin
        float cellWorldX = cellX * 4096.0f;
        float cellWorldY = cellY * 4096.0f;

        // Quadrant (qx, qy)
        int qx = vertex.quadrant % 2;
        int qy = vertex.quadrant / 2;

        // Quadrant world offset
        float quadWorldX = cellWorldX + qx * 2048.0f;
        float quadWorldY = cellWorldY + qy * 2048.0f;

        // Vertex (vx, vy) in quadrant
        int vx = vertex.vertex % 17;
        int vy = vertex.vertex / 17;

        // Vertex world offset
        float vertWorldX = quadWorldX + vx * 128.0f;
        float vertWorldY = quadWorldY + vy * 128.0f;

        float vertWorldZ = 0.0f;
        RE::TES::GetSingleton()->GetLandHeight(RE::NiPoint3{vertWorldX, vertWorldY, 0.0f}, vertWorldZ);

        return RE::NiPoint3{vertWorldX, vertWorldY, vertWorldZ};
    }

}  // namespace Utils