#pragma once

#include "material.hpp"

namespace rtweekend {

    class Lambertian final : public Material
    {
    public:
        Lambertian(const glm::vec3& albedo);
        virtual ~Lambertian() = default;

        virtual auto scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, glm::vec3& attenuation, Ray& scattered) const -> bool override;

    private:
        glm::vec3 m_albedo;
    };

}
