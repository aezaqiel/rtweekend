#pragma once

#include "sampler_integrator.hpp"
#include "sky/sky.hpp"

namespace rtweekend {

    class PathIntegrator final : public SamplerIntegrator
    {
    public:
        PathIntegrator(
            std::unique_ptr<Sampler> sampler,
            usize samples,
            usize depth,
            std::shared_ptr<Sky> sky = nullptr
        );

        virtual ~PathIntegrator() = default;

    private:
        [[nodiscard]] virtual auto li(const Ray& ray, const Hittable& world, Sampler& sampler) const -> vec3 override;

    private:
        usize m_depth { 1 };

        std::shared_ptr<Sky> m_sky;
    };

}
