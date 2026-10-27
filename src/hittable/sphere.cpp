#include "sphere.hpp"

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
        real const discriminant = a * (r2 - glm::dot(perp, perp));

        if (discriminant < 0.0) return false;

        real const sqrtd = std::sqrt(discriminant);
        real const c = glm::dot(oc, oc) - r2;

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
        record.point = ray(record.distance);
        vec3 const normal = glm::normalize(root * d - oc);
        record.set_normal(ray, normal);
        record.material = m_material;

        return true;
    }

}
