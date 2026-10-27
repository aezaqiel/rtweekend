#pragma once

#include "material.hpp"

namespace rtweekend {

    class Metal final : public Material
    {
    public:
        Metal(const vec3& albedo, real fuzz = 0.0);
        virtual ~Metal() = default;

        virtual auto scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, vec3& attenuation, Ray& scattered) const -> bool override;

    private:
        vec3 m_albedo;
        real m_fuzz { 0.0 };
    };

}
