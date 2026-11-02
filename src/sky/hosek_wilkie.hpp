#pragma once

#include <array>

#include "sky.hpp"

namespace rtweekend {

    struct HosekWilkieConfig
    {
        vec3 sun_direction          { 0.45, 0.38, 0.80 };
        real turbidity              { 2.5 };
        vec3 ground_albedo          { 0.3, 0.3, 0.3 };
        real sun_angular_radius_deg { 0.255 };
        real radiance_scale         { 0.065 };
        real sun_intensity          { 1.0 };
        bool enable_sun_disc        { true };
        bool enable_ground_bounce   { true };
    };

    class HosekWilkieSky final : public Sky
    {
    public:
        explicit HosekWilkieSky(const HosekWilkieConfig& config = {});
        virtual ~HosekWilkieSky() = default;

        [[nodiscard]] virtual auto eval(const vec3& wi) const noexcept -> vec3 override;
        [[nodiscard]] virtual auto sample(real u_select, const vec2& u_dir) const noexcept -> std::optional<SkySample> override;
        [[nodiscard]] virtual auto pdf(const vec3& wi) const noexcept -> real override;

    private:
        using ChannelConfig = std::array<real, 9>;

        [[nodiscard]] auto eval_sky_dome(const vec3& wi) const noexcept -> vec3;
        [[nodiscard]] auto eval_sun_disc(const vec3& wi, real cos_gamma, real gamma) const noexcept -> vec3;
        [[nodiscard]] auto dome_pdf(const vec3& wi) const noexcept -> real;
        [[nodiscard]] auto sun_cone_pdf(real cos_gamma) const noexcept -> real;

        auto cook_sky_configuration() noexcept -> void;
        auto precompute_solar_radiance() noexcept -> void;
        auto build_importance_sampling_tables() -> void;

    private:
        inline static constexpr usize s_cdf_theta_res { 64 };
        inline static constexpr usize s_cdf_phi_res { 128 };

        vec3 m_sun_dir { 0.0, 1.0, 0.0 };
        real m_solar_elevation { 0.5 };
        real m_turbidity { 2.5 };
        vec3 m_ground_albedo { 0.3, 0.3, 0.3 };
        real m_radiance_scale { 0.065 };
        real m_sun_intensity { 1.0 };
        bool m_enable_sun_disc { true };
        bool m_enable_ground_bounce { true };

        real m_sun_angular_radius { 0.00445058959 };
        real m_cos_sun_radius { 0.99999009628 };
        real m_sin2_sun_radius { 1.980734e-5 };
        real m_one_minus_cos_sun { 9.90372e-6 };

        std::array<ChannelConfig, 3> m_configs {};
        vec3 m_sky_radiances { 1.0, 1.0, 1.0 };
        std::array<real, 11> m_attenuated_solar_spectrum {};
        vec3 m_ground_radiance { 0.0, 0.0, 0.0 };

        real m_sun_sample_weight { 0.5 };
        std::vector<real> m_marginal_cdf;
        std::vector<real> m_conditional_cdf;
        std::vector<real> m_cell_pdf;
    };

}
