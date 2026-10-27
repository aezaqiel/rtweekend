#include "lambertian.hpp"

#include "sampler/sampler.hpp"

namespace rtweekend {

    Lambertian::Lambertian(const vec3& albedo)
        : m_albedo(albedo)
    {
    }

    auto Lambertian::scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, vec3& attenuation, Ray& scattered) const -> bool
    {
        vec3 direction = record.normal + warp::uniform_unit_sphere(sample.direction);

        real const len2 = glm::dot(direction, direction);
        if (len2 <= 1e-16) {
            direction = record.normal;
        } else {
            direction /= std::sqrt(len2);
        }

        scattered = Ray(record.point, direction);
        attenuation = m_albedo;

        return true;
    }

}
