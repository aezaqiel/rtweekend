#pragma once

#include "hittable/hittable.hpp"
#include "hittable/hittable_list.hpp"

namespace rtweekend {

    class BVH final : public Hittable
    {
    public:
        explicit BVH(const HittableList& list);
        explicit BVH(std::vector<std::shared_ptr<Hittable>> primitives);

        virtual ~BVH() = default;

        virtual auto hit(const Ray& ray, Interval t, HitRecord& record) const -> bool override;
        [[nodiscard]] virtual auto bounding_box() const noexcept -> AABB override;

        [[nodiscard]] virtual auto is_emissive() const noexcept -> bool override
        {
            return !m_emitters.empty();
        }

        [[nodiscard]] virtual auto sample_solid_angle(const vec3& origin, real u_select, const vec2& u_dir) const -> std::optional<LightSample> override;
        [[nodiscard]] virtual auto pdf_solid_angle(const vec3& origin, const vec3& wi) const -> real override;

        virtual auto collect_emitters(std::vector<const Hittable*>& out) const -> void override;

    private:
        struct LinearNode
        {
            AABB bbox       {};
            usize offset    { 0 };
            u16 count       { 0 };
            u8 axis         { 0 };
        };

    private:
        auto build_recursive(usize start, usize end) -> usize;

    private:
        std::vector<std::shared_ptr<Hittable>> m_primitives;
        std::vector<LinearNode> m_nodes;
        std::vector<const Hittable*> m_emitters;
    };

}
