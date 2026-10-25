#pragma once

#include "hittable/hittable.hpp"
#include "sampler/sampler.hpp"

namespace rtweekend {

    struct MaterialSample
    {
        f32 lobe { 0.0f };
        glm::vec2 direction { 0.0f, 0.0f };

        template <std::derived_from<Sampler> S>
        [[nodiscard]] static constexpr auto from(S& sampler) noexcept -> MaterialSample
        {
            f32 const lobe = sampler.sample_1d();
            glm::vec2 const direction = sampler.sample_2d();
            return MaterialSample { lobe, direction };
        }
    };

    class Material
    {
    public:
        virtual ~Material() = default;

        virtual auto scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, glm::vec3& attenuation, Ray& scattered) const -> bool
        {
            return false;
        }
    };

}
