#include "dielectric.hpp"

namespace rtweekend {

    Dielectric::Dielectric(f32 ri)
        : m_ri(ri)
    {
    }

    auto Dielectric::scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, glm::vec3& attenuation, Ray& scattered) const -> bool
    {
        attenuation = glm::vec3(1.0f);
        f32 const ri = record.front_face ? (1.0f / m_ri) : m_ri;

        glm::vec3 const unit_dir = glm::normalize(ray.direction());
        f32 const cos_theta = std::min(glm::dot(-unit_dir, record.normal), 1.0f);
        f32 const sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));

        bool const cannot_refract = (ri * sin_theta) > 1.0f;
        glm::vec3 direction;

        if (cannot_refract || reflectance(cos_theta, ri) > sample.lobe) {
            direction = glm::reflect(unit_dir, record.normal);
        } else {
            direction = glm::refract(unit_dir, record.normal, ri);
        }

        scattered = Ray(record.point, direction);

        return true;
    }

    auto Dielectric::reflectance(f32 cosine, f32 ri) noexcept -> f32
    {
        f32 r0 = (1.0f - ri) / (1.0f + ri);
        r0 = r0 * r0;
        return r0 + (1.0f - r0) * std::pow(1.0f - cosine, 5.0f);
    }

}
