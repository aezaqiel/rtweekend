#pragma once

#include "sampler.hpp"

namespace rtweekend {

    class IndependentSampler final : public Sampler
    {
    public:
        explicit IndependentSampler(u64 seed = 0x853c49e6748fea9bULL, u64 stream = 0xda3e39cb94b95bdbULL) noexcept;

        auto seed(u64 initstate, u64 initsequence = 1) noexcept -> void;

        virtual auto seed_pixel(usize px, usize py, usize ps) noexcept -> void override;

        [[nodiscard]] virtual auto sample_u32() noexcept -> u32 override;
        [[nodiscard]] virtual auto sample_u32(u32 bound) noexcept -> u32 override;

        [[nodiscard]] virtual auto sample_1d() noexcept -> f32 override;
        [[nodiscard]] virtual auto sample_2d() noexcept -> glm::vec2 override;

        [[nodiscard]] virtual auto uniform(f32 min, f32 max) noexcept -> f32 override;

        [[nodiscard]] inline virtual auto clone() const -> std::unique_ptr<Sampler> override
        {
            return std::make_unique<IndependentSampler>(*this);
        }

    private:
        u64 m_state { 0x853c49e6748fea9bULL };
        u64 m_increment { 0xda3e39cb94b95bdbULL };
    };

}
