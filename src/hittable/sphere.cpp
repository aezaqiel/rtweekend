#include "sphere.hpp"

namespace rtweekend {

    Sphere::Sphere(const glm::vec3& center, f32 radius)
        : m_center(center)
        , m_radius(radius)
    {
    }

    auto Sphere::hit(const Ray& ray, f32 tmin, f32 tmax, HitRecord& record) const -> bool
    {
        glm::vec3 oc = m_center - ray.origin();

        f32 a = glm::dot(ray.direction(), ray.direction());
        f32 b = -2.0f * glm::dot(ray.direction(), oc);
        f32 c = glm::dot(oc, oc) - m_radius * m_radius;

        f32 discriminant = b * b - 4.0f * a * c;

        if (discriminant < 0.0f) return false;

        f32 sqrtd = std::sqrt(discriminant);

        f32 root = (-b - sqrtd) / (2.0f * a);
        if (root <= tmin || root > tmax) {
            root = (-b + sqrtd) / (2.0f * a);
            if (root <= tmin || root > tmax) return false;
        }

        record.distance = root;
        record.point = ray(record.distance);

        glm::vec3 normal = (record.point - m_center) / m_radius;
        record.set_normal(ray, normal);

        return true;
    }

}