#pragma once

#include <memory>

#include "ray.hpp"
#include "interval.hpp"

namespace rtweekend {

    class Material;

    struct HitRecord
    {
        vec3 point {};
        vec3 normal {};
        real distance { 0.0 };
        bool front_face { false };

        std::shared_ptr<Material> material;

        inline auto set_normal(const Ray& r, const vec3& n) -> void
        {
            front_face = glm::dot(r.direction(), n) < 0.0;
            normal = front_face ? n : -n;
        }
    };

    class Hittable
    {
    public:
        virtual ~Hittable() = default;

        virtual auto hit(const Ray& ray, Interval t, HitRecord& record) const -> bool = 0;
    };

}
