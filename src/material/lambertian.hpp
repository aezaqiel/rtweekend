#pragma once

#include "material.hpp"

namespace rtweekend {

    class Lambertian final : public Material
    {
    public:
        Lambertian(const vec3& albedo);
        virtual ~Lambertian() = default;

        virtual auto scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, vec3& attenuation, Ray& scattered) const -> bool override;

    private:
        vec3 m_albedo;
    };

}
