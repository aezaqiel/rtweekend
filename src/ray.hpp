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

}
