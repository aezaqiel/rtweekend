#pragma once

#include <glm/glm.hpp>

#include "ray.hpp"

namespace rtweekend {

    struct HitRecord
    {
        glm::vec3 point;
        glm::vec3 normal;
        f32 distance;
        bool front_face;

        inline auto set_normal(const Ray& r, const glm::vec3& n) -> void
        {
            front_face = glm::dot(r.direction(), n) < 0.0f;
            normal = front_face ? n : -n;
        }
    };

    class Hittable
    {
    public:
        virtual ~Hittable() = default;

        virtual auto hit(const Ray& ray, f32 tmin, f32 tmax, HitRecord& record) const -> bool = 0;
    };

}
