#pragma once

#include "interval.hpp"
#include "ray.hpp"

namespace rtweekend {

    struct AABB
    {
        vec3 min { +std::numeric_limits<real>::infinity() };
        vec3 max { -std::numeric_limits<real>::infinity() };

        constexpr AABB() noexcept = default;
        constexpr AABB(const vec3& a, const vec3& b) noexcept
            : min(glm::min(a, b))
            , max(glm::max(a, b))
        {
        }

        [[nodiscard]] static constexpr auto empty() noexcept -> AABB
        {
            return AABB {};
        }

        [[nodiscard]] static constexpr auto merge(const AABB& a, const AABB& b) noexcept -> AABB
        {
            AABB res;
            res.min = glm::min(a.min, b.min);
            res.max = glm::max(a.max, b.max);

            return res;
        }

        [[nodiscard]] constexpr auto centroid() const noexcept -> vec3
        {
            return 0.5 * (min + max);
        }

        [[nodiscard]] constexpr auto diagonal() const noexcept -> vec3
        {
            return glm::max(max - min, vec3(0.0));
        }

        [[nodiscard]] constexpr auto surface_area() const noexcept -> real
        {
            vec3 const d = diagonal();
            return 2.0 * (d.x * d.y + d.y * d.z + d.z * d.x);
        }

        [[nodiscard]] inline auto hit(const Ray& ray, const vec3& inv_dir, Interval t) const noexcept -> bool
        {
            constexpr real gamma3 = (3.0 * std::numeric_limits<real>::epsilon()) / (1.0 - 3.0 * std::numeric_limits<real>::epsilon());
            constexpr real pad = 1.0 + 2.0 * gamma3;

            vec3 const origin = ray.origin();

            for (usize a = 0; a < 3; ++a) {
                real t0 = (min[a] - origin[a]) * inv_dir[a];
                real t1 = (max[a] - origin[a]) * inv_dir[a];

                if (t0 > t1) std::swap(t0, t1);

                t1 *= pad;

                if (t0 > t.min) t.min = t0;
                if (t1 < t.max) t.max = t1;

                if (t.max < t.min) return false;
            }

            return true;
        }
    };

}
