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
            real vfov           { 90.0 };
            vec3 lookfrom       { 0.0, 0.0,  0.0 };
            vec3 lookat         { 0.0, 0.0, -1.0 };
            vec3 vup            { 0.0, 1.0,  0.0 };
            real defocus_angle  { 0.0 };
            real focal_length   { 1.0 };
        };

    public:
        ThinLensCamera(const Config& config);
        virtual ~ThinLensCamera() = default;

        [[nodiscard]] virtual auto genray(usize i, usize j, const CameraSample& sample = {}) const -> Ray override;

    private:
        usize m_width { 0 };
        usize m_height { 0 };

        vec3 m_origin;
        vec3 m_pixel00;
        vec3 m_delta_u;
        vec3 m_delta_v;

        vec3 m_u;
        vec3 m_v;
        vec3 m_w;

        real m_defocus_angle { 0.0 };
        vec3 m_defocus_u;
        vec3 m_defocus_v;
    };

}
