#include "hittable_list.hpp"

namespace rtweekend {

    auto HittableList::hit(const Ray& ray, f32 tmin, f32 tmax, HitRecord& record) const -> bool
    {
        HitRecord scratch;
        bool hit = false;
        f32 closest = tmax;

        for (const auto& hittable : m_hittables) {
            if (hittable->hit(ray, tmin, closest, scratch)) {
                hit = true;
                closest = scratch.distance;
                record = scratch;
            }
        }

        return hit;
    }

}
