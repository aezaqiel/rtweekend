#pragma once

#include <glm/glm.hpp>

#include "ray.hpp"

namespace rtweekend {

    struct CameraSample
    {
        glm::vec2 offset    { 0.5f, 0.5f };
        glm::vec2 uv        { 0.5f, 0.5f };
        f32 time            { 0.0f };
    };

    class Camera
    {
    public:
        virtual ~Camera() = default;

        [[nodiscard]] virtual auto genray(usize i, usize j, const CameraSample& sample = {}) const -> Ray = 0;
    };

}
