#pragma once

#include "hittable/hittable.hpp"
#include "camera/camera.hpp"
#include "film/film.hpp"

namespace rtweekend {

    class Integrator
    {
    public:
        virtual ~Integrator() = default;

        virtual auto render(const Hittable& world, const Camera& camera, Film& film) const -> void = 0;
    };

}
