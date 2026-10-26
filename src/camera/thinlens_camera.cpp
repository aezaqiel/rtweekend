#include "thinlens_camera.hpp"

#include <numbers>

#include "sampler/sampler.hpp"

namespace rtweekend {

    ThinLensCamera::ThinLensCamera(const Config& config)
        : m_width(config.width)
        , m_height(config.height)
        , m_origin(config.lookfrom)
        , m_defocus_angle(config.defocus_angle)
    {
        f32 const aspect_ratio = static_cast<f32>(m_width) / static_cast<f32>(m_height);

        f32 const theta = config.vfov * (std::numbers::pi_v<f32> / 180.0f);
        f32 const h = std::tan(theta * 0.5f);

        f32 const viewport_height = 2.0f * h * config.focal_length;
        f32 const viewport_width = viewport_height * aspect_ratio;

        m_w = glm::normalize(config.lookfrom - config.lookat);
        m_u = glm::normalize(glm::cross(config.vup, m_w));
        m_v = glm::cross(m_w, m_u);

        glm::vec3 const viewport_u = viewport_width * m_u;
        glm::vec3 const viewport_v = viewport_height * (-m_v);

        m_delta_u = viewport_u / static_cast<f32>(m_width);
        m_delta_v = viewport_v / static_cast<f32>(m_height);

        glm::vec3 const viewport_origin = m_origin - (config.focal_length * m_w) - (viewport_u * 0.5f) - (viewport_v * 0.5f);
        m_pixel00 = viewport_origin + 0.5f * (m_delta_u + m_delta_v);

        f32 const defocus_radius = config.focal_length * std::tan((config.defocus_angle * 0.5f) * (std::numbers::pi_v<f32> / 180.0f));
        m_defocus_u = m_u * defocus_radius;
        m_defocus_v = m_v * defocus_radius;
    }

    [[nodiscard]] auto ThinLensCamera::genray(usize i, usize j, const CameraSample& sample) const -> Ray
    {
        glm::vec2 const offset = sample.offset - 0.5f;
        f32 const u = static_cast<f32>(i) + offset.x;
        f32 const v = static_cast<f32>(j) + offset.y;

        glm::vec3 const target = m_pixel00 + (u * m_delta_u) + (v * m_delta_v);

        glm::vec3 origin = m_origin;
        if (m_defocus_angle > 0.0f) {
            glm::vec2 const disk = warp::concentric_unit_disk(sample.uv);
            origin += (disk.x * m_defocus_u) + (disk.y * m_defocus_v);
        }

        return Ray(origin, target - origin);
    }

}
