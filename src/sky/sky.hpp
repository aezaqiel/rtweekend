#pragma once

#include <optional>

namespace rtweekend {

    struct SkySample
    {
        vec3 wi         { 0.0, 1.0, 0.0 };
        vec3 radiance   { 0.0, 0.0, 0.0 };
        real pdf        { 0.0 };
    };

    class Sky
    {
    public:
        virtual ~Sky() = default;

        [[nodiscard]] virtual auto eval(const vec3& wi) const noexcept -> vec3 = 0;

        [[nodiscard]] virtual auto sample(real u_select, const vec2& u_dir) const noexcept -> std::optional<SkySample>
        {
            return std::nullopt;
        }

        [[nodiscard]] virtual auto pdf(const vec3& wi) const noexcept -> real
        {
            return 0.0;
        }
    };

}
