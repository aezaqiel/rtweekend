#pragma once

#include "hittable.hpp"

namespace rtweekend {

    class Sphere : public Hittable
    {
    public:
        Sphere(const glm::vec3& center, f32 radius, const std::shared_ptr<Material>& material);
        virtual ~Sphere() = default;

        virtual auto hit(const Ray& ray, Interval t, HitRecord& record) const -> bool override;

    private:
        glm::vec3 m_center;
        f32 m_radius;

        std::shared_ptr<Material> m_material;
    };

}
