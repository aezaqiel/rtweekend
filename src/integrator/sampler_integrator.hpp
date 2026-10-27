#pragma once

#include "integrator.hpp"
#include "sampler/sampler.hpp"

namespace rtweekend {

    class SamplerIntegrator : public Integrator
    {
    public:
        SamplerIntegrator(std::unique_ptr<Sampler> sampler, usize sample);
        virtual ~SamplerIntegrator() = default;

        virtual auto render(const Hittable& world, const Camera& camera, Image& image) const -> void override;

    protected:
        [[nodiscard]] virtual auto li(const Ray& ray, const Hittable& world, Sampler& sampler) const -> vec3 = 0;

    protected:
        std::unique_ptr<Sampler> m_sampler;
        usize m_samples { 1 };
    };

}
