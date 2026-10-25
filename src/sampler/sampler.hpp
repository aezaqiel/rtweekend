#pragma once

#include <concepts>
#include <numbers>

#include <glm/glm.hpp>

namespace rtweekend {

    template <typename S>
    concept Sampler = requires(S& s, usize px, usize py, usize ps, u32 bound, f32 min, f32 max)
    {
        { s.seed_pixel(px, py, ps) } noexcept -> std::same_as<void>;
        { s.sample_u32() } noexcept -> std::same_as<u32>;
        { s.sample_u32(bound) } noexcept -> std::same_as<u32>;
        { s.sample_1d() } noexcept -> std::same_as<f32>;
        { s.sample_2d() } noexcept -> std::same_as<glm::vec2>;
        { s.uniform(min, max) } noexcept -> std::same_as<f32>;
    };

    namespace warp {

        [[nodiscard]] inline auto uniform_unit_sphere(const glm::vec2& u) noexcept -> glm::vec3
        {
            f32 const z = 1.0f - 2.0f * u.x;
            f32 const r = std::sqrt(std::max(0.0f, 1.0f - z * z));
            f32 const phi = 2.0f * std::numbers::pi_v<f32> * u.y;
            return glm::vec3{ r * std::cos(phi), r * std::sin(phi), z };
        }

        [[nodiscard]] inline auto concentric_unit_disk(const glm::vec2& u) noexcept -> glm::vec2
        {
            glm::vec2 const u_offset = 2.0f * u - glm::vec2(1.0f, 1.0f);
            if (u_offset.x == 0.0f && u_offset.y == 0.0f) {
                return glm::vec2(0.0f, 0.0f);
            }

            f32 theta = 0.0f;
            f32 r = 0.0f;
            if (std::abs(u_offset.x) > std::abs(u_offset.y)) {
                r = u_offset.x;
                theta = (std::numbers::pi_v<f32> / 4.0f) * (u_offset.y / u_offset.x);
            } else {
                r = u_offset.y;
                theta = (std::numbers::pi_v<f32> / 2.0f) - (std::numbers::pi_v<f32> / 4.0f) * (u_offset.x / u_offset.y);
            }
            return r * glm::vec2{ std::cos(theta), std::sin(theta) };
        }

        [[nodiscard]] inline auto cosine_hemisphere(glm::vec2 u) noexcept -> glm::vec3
        {
            glm::vec2 const d = concentric_unit_disk(u);
            f32 const z = std::sqrt(std::max(0.0f, 1.0f - d.x * d.x - d.y * d.y));
            return glm::vec3{ d.x, d.y, z };
        }

        [[nodiscard]] constexpr auto cosine_hemisphere_pdf(f32 cos_theta) noexcept -> f32
        {
            return cos_theta > 0.0f ? cos_theta * std::numbers::inv_pi_v<f32> : 0.0f;
        }

    }

}
