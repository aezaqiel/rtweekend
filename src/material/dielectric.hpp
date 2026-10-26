#pragma once

#include "material.hpp"

namespace rtweekend {

    class Dielectric final : public Material
    {
    public:
        Dielectric(f32 ri);
        virtual ~Dielectric() = default;

        virtual auto scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, glm::vec3& attenuation, Ray& scattered) const -> bool override;

    private:
        static auto reflectance(f32 cosine, f32 ri) noexcept -> f32;

    private:
        f32 m_ri { 1.0f };
    };

}
