#pragma once

#include <limits>

namespace rtweekend {

    struct Interval
    {
        f32 min { +std::numeric_limits<f32>::infinity() };
        f32 max { -std::numeric_limits<f32>::infinity() };

        inline auto size() const -> f32 { return max - min; }

        inline auto contains(f32 x) const -> bool { return min <= x && x <= max; }
        inline auto surrounds(f32 x) const -> bool { return min < x && x < max; }

        inline auto clamp(f32 x) const
        {
            if (x < min) return min;
            if (x > max) return max;
            return x;
        }

        inline static constexpr auto empty() -> Interval
        {
            return {
                +std::numeric_limits<f32>::infinity(),
                -std::numeric_limits<f32>::infinity()
            };
        }

        inline static constexpr auto universe() -> Interval
        {
            return {
                -std::numeric_limits<f32>::infinity(),
                +std::numeric_limits<f32>::infinity()
            };
        }
    };

}
