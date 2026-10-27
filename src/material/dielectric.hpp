#pragma once

#include "material.hpp"

namespace rtweekend {

    class Dielectric final : public Material
    {
    public:
        Dielectric(real ri);
        virtual ~Dielectric() = default;

        virtual auto scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, vec3& attenuation, Ray& scattered) const -> bool override;

    private:
        static auto reflectance(real cosine, real ri) noexcept -> real;

    private:
        real m_ri { 1.0 };
    };

}
