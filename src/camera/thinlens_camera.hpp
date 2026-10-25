#pragma once

#include "camera.hpp"

namespace rtweekend {

    class ThinLensCamera final : public Camera
    {
    public:
        struct Config
        {
            usize width         { 1280 };
            usize height        { 720 };
            f32 vfov            { 90.0f };
            glm::vec3 lookfrom  { 0.0f, 0.0f, 0.0f };
            glm::vec3 lookat    { 0.0f, 0.0f, -1.0f };
            glm::vec3 vup       { 0.0f, 1.0f, 0.0f };
            f32 defocus_angle   { 0.0f };
            f32 focal_length    { 1.0f };
        };

    public:
        ThinLensCamera(const Config& config);
        virtual ~ThinLensCamera() = default;

        [[nodiscard]] virtual auto genray(usize i, usize j, const CameraSample& sample = {}) const -> Ray override;

    private:
        usize m_width { 0 };
        usize m_height { 0 };

        glm::vec3 m_origin;
        glm::vec3 m_pixel00;
        glm::vec3 m_delta_u;
        glm::vec3 m_delta_v;

        glm::vec3 m_u;
        glm::vec3 m_v;
        glm::vec3 m_w;

        f32 m_defocus_angle { 0.0f };
        glm::vec3 m_defocus_u;
        glm::vec3 m_defocus_v;
    };

}
