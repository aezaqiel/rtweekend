#include "camera.hpp"

namespace rtweekend {

    Camera::Camera(usize width, usize height, const glm::vec3& origin)
        : m_width(width)
        , m_height(height)
        , m_origin(origin)
    {
        f32 aspect_ratio = static_cast<f32>(width) / static_cast<f32>(height);

        f32 focal_length = 1.0f;
        f32 viewport_height = 2.0f;
        f32 viewport_width = viewport_height * aspect_ratio;

        glm::vec3 viewport_u(viewport_width, 0.0f, 0.0f);
        glm::vec3 viewport_v(0.0f, -viewport_height, 0.0f);

        m_u = viewport_u / static_cast<f32>(width);
        m_v = viewport_v / static_cast<f32>(height);

        m_pixel00 = m_origin - glm::vec3(0.0f, 0.0f, focal_length) - viewport_u / 2.0f - viewport_v / 2.0f + 0.5f * (m_u + m_v);
    }

    auto Camera::operator()(usize i, usize j) const -> Ray
    {
        f32 u = static_cast<f32>(i);
        f32 v = static_cast<f32>(j);

        glm::vec3 center = m_pixel00 + (u * m_u) + (v * m_v);
        glm::vec3 direction = center - m_origin;

        return Ray(m_origin, direction);
    }

}
