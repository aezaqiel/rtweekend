#pragma once

#include <limits>

namespace rtweekend {

    struct Interval
    {
        real min { +std::numeric_limits<real>::infinity() };
        real max { -std::numeric_limits<real>::infinity() };

        inline auto size() const -> real { return max - min; }

        inline auto contains(real x) const -> bool { return min <= x && x <= max; }
        inline auto surrounds(real x) const -> bool { return min < x && x < max; }

        inline auto clamp(real x) const
        {
            if (x < min) return min;
            if (x > max) return max;
            return x;
        }

        inline static constexpr auto empty() -> Interval
        {
            return {
                +std::numeric_limits<real>::infinity(),
                -std::numeric_limits<real>::infinity()
            };
        }

        inline static constexpr auto universe() -> Interval
        {
            return {
                -std::numeric_limits<real>::infinity(),
                +std::numeric_limits<real>::infinity()
            };
        }
    };

}
