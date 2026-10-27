#pragma once

#include "ray.hpp"

namespace rtweekend {

    struct CameraSample
    {
        vec2 offset { 0.5, 0.5 };
        vec2 uv     { 0.5, 0.5 };
        real time   { 0.0 };
    };

    class Camera
    {
    public:
        virtual ~Camera() = default;

        [[nodiscard]] virtual auto genray(usize i, usize j, const CameraSample& sample = {}) const -> Ray = 0;
    };

}
