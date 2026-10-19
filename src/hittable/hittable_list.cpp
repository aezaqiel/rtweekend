#include "hittable_list.hpp"

namespace rtweekend {

    auto HittableList::hit(const Ray& ray, Interval t, HitRecord& record) const -> bool
    {
        HitRecord scratch;
        bool hit = false;
        f32 closest = t.max;

        for (const auto& hittable : m_hittables) {
            if (hittable->hit(ray, Interval { t.min, closest }, scratch)) {
                hit = true;
                closest = scratch.distance;
                record = scratch;
            }
        }

        return hit;
    }

}
