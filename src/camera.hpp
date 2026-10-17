#pragma once

#include <glm/glm.hpp>

#include "ray.hpp"

namespace rtweekend {

    class Camera
    {
    public:
        explicit Camera(usize width, usize height, const glm::vec3& origin);

        auto operator()(usize i, usize j) const -> Ray;

    private:
        usize m_width { 0 };
        usize m_height { 0 };
        glm::vec3 m_origin;

        glm::vec3 m_pixel00;
        glm::vec3 m_u;
        glm::vec3 m_v;
    };

}
