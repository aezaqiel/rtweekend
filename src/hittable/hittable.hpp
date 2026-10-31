#pragma once

#include <memory>
#include <optional>

#include "ray.hpp"
#include "interval.hpp"
#include "aabb.hpp"

namespace rtweekend {

    class Material;

    struct HitRecord
    {
        vec3 point {};
        vec3 normal {};
        vec3 tangent {};
        real distance { 0.0 };
        bool front_face { false };

        std::shared_ptr<Material> material;

        inline auto set_normal(const Ray& r, const vec3& n, const vec3& t = vec3(0.0)) -> void
        {
            front_face = glm::dot(r.direction(), n) < 0.0;
            normal = front_face ? n : -n;
            tangent = t;
        }
    };

    struct LightSample
    {
        vec3 wi         { 0.0 };
        vec3 point      { 0.0 };
        vec3 normal     { 0.0 };
        vec3 emission   { 0.0 };
        real distance   { 0.0 };
        real pdf        { 0.0 };
    };

    class Hittable
    {
    public:
        virtual ~Hittable() = default;

        virtual auto hit(const Ray& ray, Interval t, HitRecord& record) const -> bool = 0;

        [[nodiscard]] virtual auto bounding_box() const noexcept -> AABB
        {
            return AABB::empty();
        }

        [[nodiscard]] virtual auto is_emissive() const noexcept -> bool
        {
            return false;
        }

        [[nodiscard]] virtual auto sample_solid_angle(const vec3& origin, real u_select, const vec2& u_dir) const -> std::optional<LightSample>
        {
            return std::nullopt;
        }

        [[nodiscard]] virtual auto pdf_solid_angle(const vec3& origin, const vec3& wi) const -> real
        {
            return 0.0;
        }

        virtual auto collect_emitters(std::vector<const Hittable*>& out) const -> void
        {
            if (is_emissive()) out.push_back(this);
        }
    };

}
