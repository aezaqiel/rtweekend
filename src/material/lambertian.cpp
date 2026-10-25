#include "lambertian.hpp"

#include "sampler/sampler.hpp"

namespace rtweekend {

    Lambertian::Lambertian(const glm::vec3& albedo)
        : m_albedo(albedo)
    {
    }

    auto Lambertian::scatter(const Ray& ray, const HitRecord& record, const MaterialSample& sample, glm::vec3& attenuation, Ray& scattered) const -> bool
    {
        glm::vec3 direction = record.normal + warp::uniform_unit_sphere(sample.direction);

        if (glm::all(glm::lessThan(glm::abs(direction), glm::vec3(std::numeric_limits<f32>::epsilon())))) {
            direction = record.normal;
        }

        scattered = Ray(record.point, direction);
        attenuation = m_albedo;

        return true;
    }

}
