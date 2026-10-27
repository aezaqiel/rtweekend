#pragma once

#include "hittable/hittable.hpp"
#include "sampler/sampler.hpp"

namespace rtweekend {

    struct MaterialSample
    {
        real lobe       { 0.0 };
        vec2 direction  { 0.0, 0.0 };

        template <std::derived_from<Sampler> S>
        [[nodiscard]] static constexpr auto from(S& sampler) noexcept -> MaterialSample
        {
            real const lobe = sampler.sample_1d();
            vec2 const direction = sampler.sample_2d();
            return MaterialSample { lobe, direction };
        }
    };

    class Material
    {
    public:
        virtual ~Material() = default;

        virtual auto scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, vec3& attenuation, Ray& scattered) const -> bool
        {
            return false;
        }
    };

}
