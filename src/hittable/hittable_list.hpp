#pragma once

#include <vector>
#include <memory>

#include "hittable.hpp"

namespace rtweekend {

    class HittableList : public Hittable
    {
    public:
        HittableList() = default;
        virtual ~HittableList() = default;

        inline auto push(const std::shared_ptr<Hittable>& hittable) -> void
        {
            m_hittables.push_back(hittable);
            m_bbox = AABB::merge(m_bbox, hittable->bounding_box());

            if (hittable->is_emissive()) {
                m_emitters.push_back(hittable.get());
            }
        }

        [[nodiscard]] inline auto objects() const noexcept -> const std::vector<std::shared_ptr<Hittable>>&
        {
            return m_hittables;
        }

        virtual auto hit(const Ray& ray, Interval t, HitRecord& record) const -> bool override;

        [[nodiscard]] virtual auto bounding_box() const noexcept -> AABB override
        {
            return m_bbox;
        }

        [[nodiscard]] virtual auto is_emissive() const noexcept -> bool override
        {
            return !m_emitters.empty();
        }

        [[nodiscard]] virtual auto sample_solid_angle(const vec3& origin, real u_select, const vec2& u_dir) const -> std::optional<LightSample> override;
        [[nodiscard]] virtual auto pdf_solid_angle(const vec3& origin, const vec3& wi) const -> real override;

        virtual auto collect_emitters(std::vector<const Hittable*>& out) const -> void override;

    private:
        std::vector<std::shared_ptr<Hittable>> m_hittables;
        std::vector<const Hittable*> m_emitters;
        AABB m_bbox { AABB::empty() };
    };

}
