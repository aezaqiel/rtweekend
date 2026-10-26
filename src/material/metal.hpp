#pragma once

#include "material.hpp"

namespace rtweekend {

    class Metal final : public Material
    {
    public:
        Metal(const glm::vec3& albedo, f32 fuzz = 0.0f);
        virtual ~Metal() = default;

        virtual auto scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, glm::vec3& attenuation, Ray& scattered) const -> bool override;

    private:
        glm::vec3 m_albedo;
        f32 m_fuzz { 0.0f };
    };

}
