#pragma once

namespace Math {

    float Round(float value, int n);
    float Ceil(float value, int n);

    namespace LinAlg {
        namespace R3 {
            void rotateX(RE::NiPoint3& v, float angle);

            // Function to rotate a vector around the y-axis
            void rotateY(RE::NiPoint3& v, float angle);

            // Function to rotate a vector around the z-axis
            void rotateZ(RE::NiPoint3& v, float angle);

            void rotate(RE::NiPoint3& v, float angleX, float angleY, float angleZ);
        };

        class Geometry {
            std::vector<RE::NiPoint3> positions;
            std::vector<uint16_t> indexes;
            const RE::TESObjectREFR* obj;

            void FetchVertices(const RE::BSGeometry* o3d, RE::BSGraphics::TriShape* triShape);

        public:
            static RE::NiPoint3 Rotate(const RE::NiPoint3& A, const RE::NiPoint3& angles);

            ~Geometry() = default;
            explicit Geometry(const RE::TESObjectREFR* obj);
            [[nodiscard]] std::pair<RE::NiPoint3, RE::NiPoint3> GetBoundingBox() const;
        };

        std::array<RE::NiPoint3, 3> GetClosest3Vertices(const std::array<RE::NiPoint3, 8>& a_bounding_box,
                                                        const RE::NiPoint3& outside_point);
        std::array<RE::NiPoint3, 3> GetClosest3Vertices(const std::array<RE::NiPoint3, 4>& a_bounded_plane,
                                                        const RE::NiPoint3& outside_point);
        RE::NiPoint3 CalculateNormalOfPlane(const RE::NiPoint3& span1, const RE::NiPoint3& span2);
        RE::NiPoint3 closestPointOnPlane(const RE::NiPoint3& a_point_on_plane, const RE::NiPoint3& a_point_not_on_plane,
                                         const RE::NiPoint3& v_normal);
        RE::NiPoint3 intersectLine(const std::array<RE::NiPoint3, 3>& vertices,
                                   const RE::NiPoint3& outside_plane_point);
    };
};