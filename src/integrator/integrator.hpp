#pragma once

#include "hittable/hittable.hpp"
#include "camera.hpp"
#include "image.hpp"

namespace rtweekend {

    class Integrator
    {
    public:
        virtual ~Integrator() = default;

        virtual auto render(const Hittable& world, const Camera& camera, Image& image) const -> void = 0;
    };

}
