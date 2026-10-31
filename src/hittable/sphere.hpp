#pragma once

#include "hittable.hpp"

namespace rtweekend {

    class Sphere : public Hittable
    {
    public:
        Sphere(const vec3& center, real radius, const std::shared_ptr<Material>& material);
        virtual ~Sphere() = default;

        virtual auto hit(const Ray& ray, Interval t, HitRecord& record) const -> bool override;

        [[nodiscard]] virtual auto bounding_box() const noexcept -> AABB override;
        [[nodiscard]] virtual auto is_emissive() const noexcept -> bool override;
        [[nodiscard]] virtual auto sample_solid_angle(const vec3& origin, real u_select, const vec2& u_dir) const -> std::optional<LightSample> override;
        [[nodiscard]] virtual auto pdf_solid_angle(const vec3& origin, const vec3& wi) const -> real override;

    private:
        vec3 m_center;
        real m_radius;

        std::shared_ptr<Material> m_material;
    };

}
