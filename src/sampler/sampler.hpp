#pragma once

#include <memory>
#include <numbers>

namespace rtweekend {

    class Sampler
    {
    public:
        virtual ~Sampler() = default;

        virtual auto seed_pixel(usize px, usize py, usize ps) noexcept -> void = 0;

        [[nodiscard]] virtual auto sample_u32() noexcept -> u32 = 0;
        [[nodiscard]] virtual auto sample_u32(u32 bound) noexcept -> u32 = 0;

        [[nodiscard]] virtual auto sample_1d() noexcept -> real = 0;
        [[nodiscard]] virtual auto sample_2d() noexcept -> vec2 = 0;

        [[nodiscard]] virtual auto uniform(real min, real max) noexcept -> real = 0;

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<Sampler> = 0;
    };

    namespace warp {

        [[nodiscard]] inline auto uniform_unit_sphere(const vec2& u) noexcept -> vec3
        {
            real const z = 1.0 - 2.0 * u.x;
            real const r = std::sqrt(std::max(0.0, 1.0 - z * z));
            real const phi = 2.0 * std::numbers::pi_v<real> * u.y;
            return vec3{ r * std::cos(phi), r * std::sin(phi), z };
        }

        [[nodiscard]] inline auto concentric_unit_disk(const vec2& u) noexcept -> vec2
        {
            vec2 const u_offset = 2.0 * u - vec2(1.0, 1.0);
            if (u_offset.x == 0.0 && u_offset.y == 0.0) {
                return vec2(0.0, 0.0);
            }

            real theta = 0.0;
            real r = 0.0;

            if (std::abs(u_offset.x) > std::abs(u_offset.y)) {
                r = u_offset.x;
                theta = (std::numbers::pi_v<real> / 4.0) * (u_offset.y / u_offset.x);
            } else {
                r = u_offset.y;
                theta = (std::numbers::pi_v<real> / 2.0) - (std::numbers::pi_v<real> / 4.0) * (u_offset.x / u_offset.y);
            }
            return r * vec2{ std::cos(theta), std::sin(theta) };
        }

        [[nodiscard]] inline auto cosine_hemisphere(vec2 u) noexcept -> vec3
        {
            vec2 const d = concentric_unit_disk(u);
            real const z = std::sqrt(std::max(0.0, 1.0 - d.x * d.x - d.y * d.y));
            return vec3{ d.x, d.y, z };
        }

        [[nodiscard]] constexpr auto cosine_hemisphere_pdf(real cos_theta) noexcept -> real
        {
            return cos_theta > 0.0 ? cos_theta * std::numbers::inv_pi_v<real> : 0.0;
        }

    }

}
