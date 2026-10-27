#include "path_integrator.hpp"

#include "material/material.hpp"

namespace rtweekend {

    PathIntegrator::PathIntegrator(std::unique_ptr<Sampler> sampler, usize samples, usize depth)
        : SamplerIntegrator(std::move(sampler), samples)
        , m_depth(depth)
    {
    }

    [[nodiscard]] auto PathIntegrator::li(const Ray& ray, const Hittable& world, Sampler& sampler) const -> vec3
    {
        vec3 radiance(0.0);
        vec3 throughput(1.0);

        Ray r = ray;

        for (usize depth = 0; depth < m_depth; ++depth) {
            HitRecord record;

            if (!world.hit(r, Interval { 1e-7, std::numeric_limits<real>::infinity() }, record)) {
                vec3 const dir = glm::normalize(r.direction());
                real const a = 0.5 * (dir.y + 1.0);
                vec3 const sky = glm::mix(vec3(1.0), vec3(0.5, 0.7, 1.0), a);

                radiance += throughput * sky;
                break;
            }

            vec3 attenuation;
            Ray scattered;

            MaterialSample sample = MaterialSample::from(sampler);

            if (!record.material->scatter(r, record, sample, attenuation, scattered)) {
                break;
            }

            throughput *= attenuation;
            r = scattered;
        }

        return radiance;
    }

}
