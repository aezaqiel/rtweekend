#include "dielectric.hpp"

#include <algorithm>

namespace rtweekend {

    Dielectric::Dielectric(real ri)
        : m_ri(ri)
    {
    }

    auto Dielectric::scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, vec3& attenuation, Ray& scattered) const -> bool
    {
        attenuation = vec3(1.0);
        real const ri = record.front_face ? (1.0 / m_ri) : m_ri;

        vec3 const unit_dir = glm::normalize(ray.direction());
        real const cos_theta = std::min(glm::dot(-unit_dir, record.normal), 1.0);
        real const sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));

        bool const cannot_refract = (ri * sin_theta) > 1.0;
        vec3 direction;

        if (cannot_refract || reflectance(cos_theta, ri) > sample.lobe) {
            direction = glm::reflect(unit_dir, record.normal);
        } else {
            direction = glm::refract(unit_dir, record.normal, ri);
        }

        scattered = Ray(record.point, direction);

        return true;
    }

    auto Dielectric::reflectance(real cosine, real ri) noexcept -> real
    {
        real const cos_theta_i = std::clamp(cosine, 0.0, 1.0);
        real const sin2_theta_i = std::max(0.0, 1.0 - cos_theta_i * cos_theta_i);
        real const sin2_theta_t = ri * ri * sin2_theta_i;

        if (sin2_theta_t >= 1.0) {
            return 1.0;
        }

        real const cos_theta_t = std::sqrt(std::max(0.0, 1.0 - sin2_theta_t));
        real const r_perp = (ri * cos_theta_i - cos_theta_t) / (ri * cos_theta_i + cos_theta_t);
        real const r_parl = (cos_theta_i - ri * cos_theta_t) / (cos_theta_i + ri * cos_theta_t);

        return 0.5 * (r_perp * r_perp + r_parl * r_parl);
    }

}
