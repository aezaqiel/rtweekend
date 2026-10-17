#pragma once

#include <glm/glm.hpp>

namespace rtweekend {

    class Ray
    {
    public:
        explicit constexpr Ray() = default;
        explicit constexpr Ray(const glm::vec3& origin, const glm::vec3& direction)
            : m_origin(origin), m_direction(direction)
        {
        }

        inline auto origin() const -> glm::vec3 { return m_origin; }
        inline auto direction() const -> glm::vec3 { return m_direction; }

        inline auto operator()(f32 t) const
        {
            return m_origin + (m_direction * t);
        }

    private:
        glm::vec3 m_origin;
        glm::vec3 m_direction;
    };

}
