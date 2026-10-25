#include "path_integrator.hpp"

#include "material/material.hpp"

namespace rtweekend {

    PathIntegrator::PathIntegrator(std::unique_ptr<Sampler> sampler, usize samples, usize depth)
        : SamplerIntegrator(std::move(sampler), samples)
        , m_depth(depth)
    {
    }

    [[nodiscard]] auto PathIntegrator::li(const Ray& ray, const Hittable& world, Sampler& sampler) const -> glm::vec3
    {
        glm::vec3 radiance(0.0f);
        glm::vec3 throughput(1.0f);

        Ray r = ray;

        for (usize depth = 0; depth < m_depth; ++depth) {
            HitRecord record;

            if (!world.hit(r, Interval { 0.001f, std::numeric_limits<f32>::infinity() }, record)) {
                glm::vec3 const dir = glm::normalize(r.direction());
                f32 const a = 0.5f * (dir.y + 1.0f);
                glm::vec3 const sky = glm::mix(glm::vec3(1.0f), glm::vec3(0.5f, 0.7f, 1.0f), a);

                radiance += throughput * sky;
                break;
            }

            glm::vec3 attenuation;
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
