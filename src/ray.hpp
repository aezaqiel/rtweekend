#pragma once

namespace rtweekend {

    class Ray
    {
    public:
        explicit constexpr Ray() = default;
        explicit constexpr Ray(const vec3& origin, const vec3& direction)
            : m_origin(origin), m_direction(direction)
        {
        }

        inline auto origin() const -> vec3 { return m_origin; }
        inline auto direction() const -> vec3 { return m_direction; }

        inline auto operator()(real t) const
        {
            return m_origin + (m_direction * t);
        }

    private:
        vec3 m_origin;
        vec3 m_direction;
    };

    [[nodiscard]] inline auto offset_ray_origin(const vec3& p, const vec3& n, const vec3& w) noexcept -> vec3
    {
        constexpr real origin_thresh    = 1.0 / 32.0;
        constexpr real float_scale      = 1.0 / 65536.0;
        constexpr real ulp_scale        = 256.0 * std::numeric_limits<real>::epsilon();

        vec3 const offset_n = (glm::dot(w, n) >= 0.0) ? n : -n;
        vec3 result;

        for (usize i = 0; i < 3; ++i) {
            real const d = offset_n[i];
            if (std::abs(p[i]) < origin_thresh) {
                result[i] = p[i] + d * float_scale;
            } else {
                real const delta = std::abs(p[i] * d) * ulp_scale;
                result[i] = std::nextafter(
                    p[i] + std::copysign(delta, d),
                    std::copysign(std::numeric_limits<real>::infinity(), d)
                );
            }
        }

        return result;
    }

}
