#include "path_integrator.hpp"

#include <algorithm>

#include "material/material.hpp"
#include "sky/hosek_wilkie.hpp"

namespace rtweekend {

    namespace {

        [[nodiscard]] constexpr auto power_heuristic(real pdf_a, real pdf_b) noexcept -> real
        {
            real const a2 = pdf_a * pdf_a;
            real const b2 = pdf_b * pdf_b;
            real const denom = a2 + b2;

            return (denom > 0.0) ? (a2 / denom) : 0.0;
        }
    
    }

    PathIntegrator::PathIntegrator(std::shared_ptr<Sampler> sampler, usize samples, usize depth, std::shared_ptr<Sky> sky)
        : SamplerIntegrator(sampler, samples)
        , m_depth(depth)
        , m_sky(sky != nullptr ? std::move(sky) : std::make_shared<HosekWilkieSky>())
    {
    }

    [[nodiscard]] auto PathIntegrator::li(const Ray& ray, const Hittable& world, Sampler& sampler) const -> vec3
    {
        vec3 radiance(0.0);
        vec3 throughput(1.0);

        Ray r = ray;

        bool prev_is_delta = true;
        real prev_bsdf_pdf = 0.0;
        vec3 prev_point(0.0);

        for (usize depth = 0; depth < m_depth; ++depth) {
            HitRecord record;

            if (!world.hit(r, Interval { 0.0, std::numeric_limits<real>::infinity() }, record)) {
                vec3 const dir = glm::normalize(r.direction());
                vec3 const sky_le = m_sky->eval(dir);

                if (sky_le.x > 0.0 || sky_le.y > 0.0 || sky_le.z > 0.0) {
                    if (depth == 0 || prev_is_delta) {
                        radiance += throughput * sky_le;
                    } else {
                        real const pdf_sky = m_sky->pdf(dir);
                        real const mis_w = power_heuristic(prev_bsdf_pdf, pdf_sky);
                        radiance += throughput * sky_le * mis_w;
                    }
                }

                break;
            }

            if (!record.front_face && record.material != nullptr) {
                vec3 const sigma_a = record.material->interior_sigma_a();
                if (sigma_a.x > 0.0 || sigma_a.y > 0.0 || sigma_a.z > 0.0) {
                    real const dist = record.distance * glm::length(r.direction());
                    throughput *= glm::exp(-sigma_a * dist);
                }
            }

            vec3 const le = record.material->emission(record);
            if (le.x > 0.0 || le.y > 0.0 || le.z > 0.0) {
                if (depth == 0 || prev_is_delta) {
                    radiance += throughput * le;
                } else {
                    vec3 const wi_prev = glm::normalize(r.direction());
                    real const pdf_light = world.pdf_solid_angle(prev_point, wi_prev);
                    real const mis_w = power_heuristic(prev_bsdf_pdf, pdf_light);

                    radiance += throughput * le * mis_w;
                }
            }

            vec3 const wo_world = -glm::normalize(r.direction());

            if (world.is_emissive()) {
                real const u_light = sampler.sample_1d();
                vec2 const u_dir = sampler.sample_2d();

                if (auto const ls = world.sample_solid_angle(record.point, u_light, u_dir); ls.has_value() && ls->pdf > 0.0) {
                    vec3 const f_bsdf = record.material->eval(wo_world, ls->wi, record);
                    if (f_bsdf.x > 0.0 || f_bsdf.y > 0.0 || f_bsdf.z > 0.0) {
                        vec3 const shadow_origin = offset_ray_origin(record.point, record.normal, ls->wi);
                        vec3 const shadow_target = offset_ray_origin(ls->point, ls->normal, -ls->wi);
                        real const shadow_distance = glm::length(shadow_target - shadow_origin);

                        HitRecord shadow_hit;
                        if (!world.hit(Ray(shadow_origin, ls->wi), Interval { 0.0, shadow_distance }, shadow_hit)) {
                            real const pdf_bsdf = record.material->pdf(wo_world, ls->wi, record);
                            real const mis_w = power_heuristic(ls->pdf, pdf_bsdf);
                            real const cos_theta_i = std::abs(glm::dot(record.normal, ls->wi));

                            radiance += throughput * f_bsdf * ls->emission * ((cos_theta_i * mis_w) / ls->pdf);
                        }
                    }
                }
            }

            real const u_sky_lobe = sampler.sample_1d();
            vec2 const u_sky_dir = sampler.sample_2d();

            if (auto const ss = m_sky->sample(u_sky_lobe, u_sky_dir); ss.has_value() && ss->pdf > 0.0) {
                vec3 const f_bsdf = record.material->eval(wo_world, ss->wi, record);
                if (f_bsdf.x > 0.0 || f_bsdf.y > 0.0 || f_bsdf.z > 0.0) {
                    vec3 const shadow_origin = offset_ray_origin(record.point, record.normal, ss->wi);

                    HitRecord shadow_hit;
                    if (!world.hit(Ray(shadow_origin, ss->wi), Interval { 0.0, std::numeric_limits<real>::infinity() }, shadow_hit)) {
                        real const pdf_bsdf = record.material->pdf(wo_world, ss->wi, record);
                        real const mis_w = power_heuristic(ss->pdf, pdf_bsdf);
                        real const cos_theta_i = std::abs(glm::dot(record.normal, ss->wi));

                        radiance += throughput * f_bsdf * ss->radiance * ((cos_theta_i * mis_w) / ss->pdf);
                    }
                }
            }

            MaterialSample const sample = MaterialSample::from(sampler);
            auto const bsdf = record.material->sample(r, record, sample);
            if (!bsdf.has_value()) break;

            throughput *= bsdf->weight;
            prev_is_delta = bsdf->is_delta;
            prev_bsdf_pdf = bsdf->pdf;
            prev_point = record.point;

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
