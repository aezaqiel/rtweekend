#pragma once

#include "hittable.hpp"

namespace rtweekend {

    class Sphere : public Hittable
    {
    public:
        Sphere(const vec3& center, real radius, const std::shared_ptr<Material>& material);
        virtual ~Sphere() = default;

        virtual auto hit(const Ray& ray, Interval t, HitRecord& record) const -> bool override;

    private:
        vec3 m_center;
        real m_radius;

        std::shared_ptr<Material> m_material;
    };

}
