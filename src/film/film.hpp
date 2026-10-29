#pragma once

#include <mutex>

namespace rtweekend {

    class PixelAccumulator
    {
    public:
        PixelAccumulator() = default;

        [[nodiscard]] inline auto weighted_sum() const noexcept -> vec3 { return m_weighted_sum; }
        [[nodiscard]] inline auto sum_compensation() const noexcept -> vec3 { return m_sum_compensation; }
        [[nodiscard]] inline auto weight_sum() const noexcept -> real { return m_weight_sum; }
        [[nodiscard]] inline auto weight_compensation() const noexcept -> real { return m_weight_compensation; }

        [[nodiscard]] inline auto sample_count() const noexcept -> u64 { return m_sample_count; }
        [[nodiscard]] inline auto mean() const noexcept -> vec3 { return m_mean; }
        [[nodiscard]] inline auto m2() const noexcept -> vec3 { return m_m2; }

        auto add_weighted(const vec3& L, real w) noexcept -> void;
        auto add_welford(const vec3& L) noexcept -> void;
        auto merge(const PixelAccumulator& other) noexcept -> void;

        [[nodiscard]] auto resolved_radiance() const noexcept -> vec3;
        [[nodiscard]] auto standard_error() const noexcept -> vec3;

    private:
        vec3 m_weighted_sum           { 0.0 };
        vec3 m_sum_compensation       { 0.0 };
        real m_weight_sum             { 0.0 };
        real m_weight_compensation    { 0.0 };

        u64  m_sample_count   { 0 };
        vec3 m_mean           { 0.0 };
        vec3 m_m2             { 0.0 };
    };

    class FilmTile
    {
    public:
        FilmTile(usize width, usize height, usize x0, usize y0, usize x1, usize y1);

        auto add_sample(const vec2& p_film, const vec3& radiance) noexcept -> void;

        [[nodiscard]] inline auto width() const noexcept -> usize { return m_width; }
        [[nodiscard]] inline auto height() const noexcept -> usize { return m_height; }

        [[nodiscard]] inline auto x0() const noexcept -> usize { return m_x0; }
        [[nodiscard]] inline auto y0() const noexcept -> usize { return m_y0; }

        [[nodiscard]] inline auto pixels() const noexcept -> const std::vector<PixelAccumulator>& { return m_pixels; }

    private:
        usize m_width  { 0 };
        usize m_height { 0 };

        usize m_x0 { 0 };
        usize m_y0 { 0 };
        usize m_x1 { 0 };
        usize m_y1 { 0 };

        std::vector<PixelAccumulator> m_pixels;
    };

    class Film
    {
    public:
        Film(usize width, usize height, real exposure_ev = 0.0);

        Film(const Film&) = delete;
        auto operator=(const Film&) -> Film& = delete;

        [[nodiscard]] inline auto width() const noexcept -> usize { return m_width; }
        [[nodiscard]] inline auto height() const noexcept -> usize { return m_height; }
        [[nodiscard]] inline constexpr auto channels() const noexcept -> usize { return s_channels; }

        [[nodiscard]] auto create_tile(usize x0, usize y0, usize x1, usize y1) const -> FilmTile;
        auto merge_tile(const FilmTile& tile) -> void;

        auto set(usize x, usize y, const vec3& rgb) -> void;
        auto set(usize x, usize y, const vec4& rgba) -> void;

        auto save(const std::string& filename) const -> void;

    private:
        [[nodiscard]] auto develop(std::vector<vec3>* out_linear_hdr = nullptr) const -> std::vector<vec3>;

    private:
        inline static constexpr usize s_channels { 4 };

    private:
        usize m_width  { 0 };
        usize m_height { 0 };

        real m_exposure_scale { 1.0 };

        std::vector<PixelAccumulator> m_pixels;

        mutable std::mutex m_mutex;
    };

}
