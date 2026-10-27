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
        real const aspect_ratio = static_cast<real>(m_width) / static_cast<real>(m_height);

        real const theta = config.vfov * (std::numbers::pi_v<real> / 180.0);
        real const h = std::tan(theta * 0.5);

        real const viewport_height = 2.0 * h * config.focal_length;
        real const viewport_width = viewport_height * aspect_ratio;

        m_w = glm::normalize(config.lookfrom - config.lookat);
        m_u = glm::normalize(glm::cross(config.vup, m_w));
        m_v = glm::cross(m_w, m_u);

        vec3 const viewport_u = viewport_width * m_u;
        vec3 const viewport_v = viewport_height * (-m_v);

        m_delta_u = viewport_u / static_cast<real>(m_width);
        m_delta_v = viewport_v / static_cast<real>(m_height);

        vec3 const viewport_origin = m_origin - (config.focal_length * m_w) - (viewport_u * 0.5) - (viewport_v * 0.5);
        m_pixel00 = viewport_origin + 0.5 * (m_delta_u + m_delta_v);

        real const defocus_radius = config.focal_length * std::tan((config.defocus_angle * 0.5) * (std::numbers::pi_v<real> / 180.0));
        m_defocus_u = m_u * defocus_radius;
        m_defocus_v = m_v * defocus_radius;
    }

    [[nodiscard]] auto ThinLensCamera::genray(usize i, usize j, const CameraSample& sample) const -> Ray
    {
        vec2 const offset = sample.offset - 0.5;
        real const u = static_cast<real>(i) + offset.x;
        real const v = static_cast<real>(j) + offset.y;

        vec3 const target = m_pixel00 + (u * m_delta_u) + (v * m_delta_v);

        vec3 origin = m_origin;
        if (m_defocus_angle > 0.0) {
            vec2 const disk = warp::concentric_unit_disk(sample.uv);
            origin += (disk.x * m_defocus_u) + (disk.y * m_defocus_v);
        }

        return Ray(origin, target - origin);
    }

}
