#include "metal.hpp"

#include <algorithm>

namespace rtweekend {

    Metal::Metal(const vec3& albedo, real fuzz)
        : m_albedo(albedo)
        , m_fuzz(std::clamp(fuzz, 0.0, 1.0))
    {
    }

    auto Metal::scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, vec3& attenuation, Ray& scattered) const -> bool
    {
        vec3 reflected = glm::reflect(glm::normalize(ray.direction()), record.normal);
        reflected = glm::normalize(reflected) + (m_fuzz * warp::uniform_unit_sphere(sample.direction));

        scattered = Ray(record.point, reflected);
        attenuation = m_albedo;

        return glm::dot(scattered.direction(), record.normal) > 0.0;
    }

}
