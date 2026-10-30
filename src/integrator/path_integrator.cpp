#include "path_integrator.hpp"

#include <algorithm>

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

            if (!world.hit(r, Interval { 0.0, std::numeric_limits<real>::infinity() }, record)) {
                vec3 const dir = glm::normalize(r.direction());
                real const a = 0.5 * (dir.y + 1.0);
                vec3 const sky = glm::mix(vec3(1.0), vec3(0.5, 0.7, 1.0), a);

                radiance += throughput * sky;

                break;
            }

            if (!record.front_face && record.material != nullptr) {
                vec3 const sigma_a = record.material->interior_sigma_a();
                if (sigma_a.x > 0.0 || sigma_a.y > 0.0 || sigma_a.z > 0.0) {
                    real const dist = record.distance * glm::length(r.direction());
                    throughput *= glm::exp(-sigma_a * dist);
                }
            }

            radiance += throughput * record.material->emission(record);

            MaterialSample const sample = MaterialSample::from(sampler);
            auto const bsdf = record.material->sample(r, record, sample);

            if (!bsdf.has_value()) break;

            throughput *= bsdf->weight;

            if (depth >= 4) {
                real const max_comp = std::max({ throughput.x, throughput.y, throughput.z });
                if (max_comp <= 0.0) break;

                real const survival = std::clamp(max_comp, 0.05, 0.95);
                if (sampler.sample_1d() >= survival) break;

                throughput /= survival;
            }

            vec3 const origin = offset_ray_origin(record.point, record.normal, bsdf->direction);
            r = Ray(origin, bsdf->direction);
        }

        return radiance;
    }

}
