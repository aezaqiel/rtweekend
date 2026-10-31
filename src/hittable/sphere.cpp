#include "sphere.hpp"

#include "material/material.hpp"

namespace rtweekend {

    Sphere::Sphere(const vec3& center, real radius, const std::shared_ptr<Material>& material)
        : m_center(center)
        , m_radius(std::max(0.0, radius))
        , m_material(material)
    {
    }

    auto Sphere::hit(const Ray& ray, Interval t, HitRecord& record) const -> bool
    {
        vec3 const d = ray.direction();
        real const a = glm::dot(d, d);

        if (a <= 0.0) return false;

        vec3 const oc = m_center - ray.origin();
        real const h = glm::dot(d, oc);
        real const r2 = m_radius * m_radius;

        // Hearn-Baker / Haines perpendicular projection discriminant:
        // Avoids catastrophic cancellation in (|oc|^2 - r^2) when |oc| >> r or |oc| ~ r
        vec3 const perp = oc - (h / a) * d;
        real const perp2 = glm::dot(perp, perp);
        real const discriminant = a * (r2 - perp2);

        if (discriminant < 0.0) return false;

        real const sqrtd = std::sqrt(discriminant);

        real const len_oc = glm::length(oc);
        real const c = (len_oc - m_radius) * (len_oc + m_radius);

        // Citardauq stable quadratic roots: avoids subtracting nearly equal h and sqrtd
        real const q = (h >= 0.0) ? (h + sqrtd) : (h - sqrtd);
        real root0 = 0.0;
        real root1 = 0.0;

        if (q != 0.0) {
            root0 = c / q;
            root1 = q / a;
            if (root0 > root1) {
                std::swap(root0, root1);
            }
        }

        real root = root0;
        if (!t.surrounds(root)) {
            root = root1;
            if (!t.surrounds(root)) return false;
        }

        record.distance = root;

        vec3 const outward_normal = glm::normalize(root * d - oc);
        record.point = m_center + m_radius * outward_normal;

        real const xz_len2 = outward_normal.x * outward_normal.x + outward_normal.z * outward_normal.z;
        vec3 const tangent = (xz_len2 > 1e-16)
            ? vec3(-outward_normal.z, 0.0, outward_normal.x) / std::sqrt(xz_len2)
            : vec3(1.0, 0.0, 0.0);

        record.set_normal(ray, outward_normal, tangent);
        record.material = m_material;

        return true;
    }

    [[nodiscard]] auto Sphere::bounding_box() const noexcept -> AABB
    {
        vec3 const rvec(m_radius);
        return AABB(m_center - rvec, m_center + rvec);
    }

    [[nodiscard]] auto Sphere::is_emissive() const noexcept -> bool
    {
        if (!m_material) return false;
        HitRecord scratch {};
        vec3 const le = m_material->emission(scratch);

        return (le.x > 0.0 || le.y > 0.0 || le.z > 0.0) && m_radius > 0.0;
    }

    [[nodiscard]] auto Sphere::sample_solid_angle(const vec3& origin, real u_select, const vec2& u_dir) const -> std::optional<LightSample>
    {
        if (!is_emissive()) return std::nullopt;

        HitRecord scratch {};
        vec3 const le = m_material->emission(scratch);
        
        vec3 const wc_vec = m_center - origin;
        real const dc2 = glm::dot(wc_vec, wc_vec);
        real const r2 = m_radius * m_radius;

        if (dc2 <= r2) {
            vec3 const n = warp::uniform_unit_sphere(u_dir);
            vec3 const p_light = m_center + m_radius * n;
            vec3 const to_light = p_light - origin;
            real const dist2 = glm::dot(to_light, to_light);

            if (dist2 <= 1e-16) return std::nullopt;

            real const dist = std::sqrt(dist2);
            vec3 const wi = to_light / dist;
            real const cos_light = std::abs(glm::dot(n, -wi));

            if (cos_light <= 1e-12) return std::nullopt;

            real const pdf = dist2 / (4.0 * std::numbers::pi_v<real> * r2 * cos_light);

            return LightSample {
                .wi = wi,
                .point = p_light,
                .normal = n,
                .emission = le,
                .distance = dist,
                .pdf = pdf
            };
        }

        real const dc = std::sqrt(dc2);
        vec3 const wc = wc_vec / dc;

        ShadingFrame const frame(wc);

        real const sin_theta_max2 = r2 / dc2;
        real const cos_theta_max = std::sqrt(std::max(0.0, 1.0 - sin_theta_max2));
        real const one_minus_cos_max = sin_theta_max2 / (1.0 + cos_theta_max);

        real const one_minus_cos = u_dir.x * one_minus_cos_max;
        real const cos_theta = 1.0 - one_minus_cos;
        real const sin_theta2 = std::max(0.0, one_minus_cos * (2.0 - one_minus_cos));
        real const sin_theta = std::sqrt(sin_theta2);
        real const phi = 2.0 * std::numbers::pi_v<real> * u_dir.y;

        real const ds = dc * cos_theta - std::sqrt(std::max(0.0, r2 - dc2 * sin_theta2));
        real const cos_alpha = (dc2 + r2 - ds * ds) / (2.0 * dc * m_radius);
        real const sin_alpha = std::sqrt(std::max(0.0, 1.0 - cos_alpha * cos_alpha));

        vec3 const n_local(sin_alpha * std::cos(phi), sin_alpha * std::sin(phi), -cos_alpha);

        vec3 const n_world = glm::normalize(frame.to_world(n_local));
        vec3 const p_light = m_center + m_radius * n_world;

        vec3 const to_light = p_light - origin;
        real const dist = glm::length(to_light);

        if (dist <= 1e-16) return std::nullopt;

        real const pdf = 1.0 / (2.0 * std::numbers::pi_v<real> * one_minus_cos_max);

        return LightSample {
            .wi = to_light / dist,
            .point = p_light,
            .normal = n_world,
            .emission = le,
            .distance = dist,
            .pdf = pdf
        };
    }

    [[nodiscard]] auto Sphere::pdf_solid_angle(const vec3& origin, const vec3& wi) const -> real
    {
        if (!is_emissive()) return 0.0;

        HitRecord record;
        if (!this->hit(Ray(origin, wi), Interval { 0.0, std::numeric_limits<real>::infinity() }, record)) {
            return 0.0;
        }

        vec3 const wc_vec = m_center - origin;
        real const dc2 = glm::dot(wc_vec, wc_vec);
        real const r2 = m_radius * m_radius;

        if (dc2 <= r2) {
            real const dist2 = record.distance * record.distance * glm::dot(wi, wi);
            real const cos_light = std::abs(glm::dot(record.normal, -glm::normalize(wi)));

            if (cos_light <= 1e-12) return 0.0;

            return dist2 / (4.0 * std::numbers::pi_v<real> * r2 * cos_light);
        }

        real const sin_theta_max2 = r2 / dc2;
        real const cos_theta_max = std::sqrt(std::max(0.0, 1.0 - sin_theta_max2));
        real const one_minus_cos_max = sin_theta_max2 / (1.0 + cos_theta_max);

        return 1.0 / (2.0 * std::numbers::pi_v<real> * one_minus_cos_max);
    }

}
