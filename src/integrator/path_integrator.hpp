#pragma once

#include "sampler_integrator.hpp"

namespace rtweekend {

    class PathIntegrator final : public SamplerIntegrator
    {
    public:
        PathIntegrator(std::unique_ptr<Sampler> sampler, usize samples, usize depth);
        virtual ~PathIntegrator() = default;

    private:
        [[nodiscard]] virtual auto li(const Ray& ray, const Hittable& world) const -> glm::vec3 override;

    private:
        usize m_depth { 1 };
    };

}
