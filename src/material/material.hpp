#pragma once

#include <optional>

#include "hittable/hittable.hpp"
#include "sampler/sampler.hpp"

namespace rtweekend {

    struct ShadingFrame
    {
        vec3 t;
        vec3 b;
        vec3 n;

        explicit constexpr ShadingFrame(const vec3& normal) noexcept
            : n(normal)
        {
            real const sign = std::copysign(1.0, n.z);
            real const a = -1.0 / (sign + n.z);
            real const d = n.x * n.y * a;

            t = vec3(1.0 + sign * n.x * n.x * a, sign * d, -sign * n.x);
            b = vec3(d, sign + n.y * n.y * a, -n.y);
        }

        explicit ShadingFrame(const HitRecord& record) noexcept
            : n(record.normal)
        {
            vec3 const t_proj = record.tangent - glm::dot(record.tangent, n) * n;
            real const len2 = glm::dot(t_proj, t_proj);

            if (len2 > 1e-16) {
                t = t_proj / std::sqrt(len2);
                b = glm::cross(n, t);
            } else {
                real const sign = std::copysign(1.0, n.z);
                real const a = -1.0 / (sign + n.z);
                real const d = n.x * n.y * a;

                t = vec3(1.0 + sign * n.z * n.z * a, sign * d, -sign * n.x);
                b = vec3(d, sign + n.y * n.y * a, -n.y);
            }
        }

        [[nodiscard]] constexpr auto to_local(const vec3& v) const noexcept -> vec3
        {
            return vec3(glm::dot(v, t), glm::dot(v, b), glm::dot(v, n));
        }

        [[nodiscard]] constexpr auto to_world(const vec3& v) const noexcept -> vec3
        {
            return t * v.x + b * v.y + n * v.z;
        }
    };

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

    struct BSDFSample
    {
        vec3 direction  { 0.0 };
        vec3 weight     { 0.0 };
        real pdf        { 0.0 };
        bool is_delta   { false };
    };

    class Material
    {
    public:
        virtual ~Material() = default;

        [[nodiscard]] virtual auto sample(const Ray& ray, const HitRecord& record, const MaterialSample& sample) const -> std::optional<BSDFSample>
        {
            return std::nullopt;
        }

        [[nodiscard]] virtual auto eval(const vec3& wo_world, const vec3& wi_world, const HitRecord& record) const -> vec3
        {
            return vec3(0.0);
        }

        [[nodiscard]] virtual auto pdf(const vec3& wo_world, const vec3& wi_world, const HitRecord& record) const -> real
        {
            return 0.0;
        }

        [[nodiscard]] virtual auto emission(const HitRecord& record) const noexcept -> vec3
        {
            return vec3(0.0);
        }

        [[nodiscard]] virtual auto interior_sigma_a() const noexcept -> vec3
        {
            return vec3(0.0);
        }

        virtual auto scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, vec3& attenuation, Ray& scattered) const -> bool
        {
            auto const bsdf = this->sample(ray, record, sample);
            if (!bsdf.has_value()) return false;

            attenuation = bsdf->weight;
            scattered = Ray(offset_ray_origin(record.point, record.normal, bsdf->direction), bsdf->direction);

            return true;
        }
    };
}
