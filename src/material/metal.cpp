#include "metal.hpp"

namespace rtweekend {

    Metal::Metal(const glm::vec3& albedo)
        : m_alebdo(albedo)
    {
    }

    auto Metal::scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, glm::vec3& attenuation, Ray& scattered) const -> bool
    {
        glm::vec3 reflected = glm::reflect(glm::normalize(ray.direction()), record.normal);

        scattered = Ray(record.point, reflected);
        attenuation = m_alebdo;

        return true;
    }

}
