#include "independent_sampler.hpp"

#include <bit>

namespace rtweekend {

    namespace {

        [[nodiscard]] constexpr auto splitmix64(u64 x) noexcept
        {
            x += 0x9e3779b97f4a7c15ULL;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            return x ^ (x >> 31);
        }

    }

    IndependentSampler::IndependentSampler(u64 seed, u64 stream) noexcept
    {
        this->seed(seed, stream);
    }

    auto IndependentSampler::seed(u64 initstate, u64 initsequence) noexcept -> void
    {
        m_state = 0U;
        m_increment = (initsequence << 1U) | 1U;
        (void)sample_u32();
        m_state += initstate;
        (void)sample_u32();
    }

    auto IndependentSampler::seed_pixel(usize px, usize py, usize ps) noexcept -> void
    {
        u64 const pixel_id = (static_cast<u64>(px) << 32) | static_cast<u64>(py);
        u64 const s0 = splitmix64(pixel_id);
        u64 const s1 = splitmix64(static_cast<u64>(ps) ^ s0);
        seed(s1, s0);
    }

    [[nodiscard]] auto IndependentSampler::sample_u32() noexcept -> u32
    {
        u64 const oldstate = m_state;
        m_state = oldstate * 6364136223846793005ULL + m_increment;
        u32 const xorshifted = static_cast<u32>(((oldstate >> 18U) ^ oldstate) >> 27U);
        u32 const rot = static_cast<u32>(oldstate >> 59U);
        return std::rotr(xorshifted, static_cast<int>(rot));
    }

    [[nodiscard]] auto IndependentSampler::sample_u32(u32 bound) noexcept -> u32
    {
        if (bound <= 1U) return 0U;
        u32 const threshold = (~bound + 1U) % bound; // Equivalent to (-bound) % bound
        for (;;) {
            u32 const r = sample_u32();
            u64 const m = static_cast<u64>(r) * static_cast<u64>(bound);
            u32 const l = static_cast<u32>(m);
            if (l >= threshold) {
                return static_cast<u32>(m >> 32U);
            }
        }
    }

    [[nodiscard]] auto IndependentSampler::sample_1d() noexcept -> real
    {
        u64 const hi = static_cast<u64>(sample_u32() >> 5U); // 27 bits
        u64 const lo = static_cast<u64>(sample_u32() >> 6U); // 26 bits
        return static_cast<real>((hi << 26U) | lo) * 0x1.0p-53;
    }

    [[nodiscard]] auto IndependentSampler::sample_2d() noexcept -> vec2
    {
        real const u = sample_1d();
        real const v = sample_1d();
        return vec2 { u, v };
    }

    [[nodiscard]] auto IndependentSampler::uniform(real min, real max) noexcept -> real
    {
        real const res = min + sample_1d() * (max - min);
        return std::min(res, std::nextafter(max, min));
    }
    
}
