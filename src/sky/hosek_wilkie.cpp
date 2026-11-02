#include "hosek_wilkie.hpp"

#include <algorithm>
#include <numbers>

#include "hosek_wilkie_data.hpp"
#include "material/material.hpp"

namespace rtweekend {

    namespace {

        [[nodiscard]] constexpr auto sqr(real x) noexcept -> real
        {
            return x * x;
        }

        [[nodiscard]] constexpr auto rec709_luminance(const vec3& rgb) noexcept -> real
        {
            return 0.2126 * rgb.x + 0.7152 * rgb.y + 0.0722 * rgb.z;
        }

        // Convert CIE 1931 XYZ (D65 white reference) to linear Rec.709 / sRGB
        [[nodiscard]] constexpr auto cie_xyz_to_linear_rec709(const vec3& xyz) noexcept -> vec3
        {
            return vec3(
                 3.2404542 * xyz.x - 1.5371385 * xyz.y - 0.4985314 * xyz.z,
                -0.9692660 * xyz.x + 1.8760108 * xyz.y + 0.0415560 * xyz.z,
                 0.0556434 * xyz.x - 0.2040259 * xyz.y + 1.0572252 * xyz.z
            );
        }

        // Degree-5 Bernstein basis weights B_{k,5}(x) = C(5, k) * (1 - x)^(5 - k) * x^k
        [[nodiscard]] constexpr auto bernstein_basis_5(real x) noexcept -> std::array<real, 6>
        {
            real const omx  = 1.0 - x;
            real const omx2 = omx * omx;
            real const omx3 = omx2 * omx;
            real const omx4 = omx2 * omx2;
            real const omx5 = omx4 * omx;

            real const x2 = x * x;
            real const x3 = x2 * x;
            real const x4 = x2 * x2;
            real const x5 = x4 * x;

            return {
                omx5,
                5.0  * omx4 * x,
                10.0 * omx3 * x2,
                10.0 * omx2 * x3,
                5.0  * omx  * x4,
                x5
            };
        }

        struct FullChannelDataset
        {
            // Layout matching ArHosekSkyModel.c:
            // config_data: [2 albedos] * [10 turbidities] * [6 Bernstein] * [9 params] = 1080 reals
            // rad_data:    [2 albedos] * [10 turbidities] * [6 Bernstein]              = 120 reals
            std::array<std::array<real, 1080>, 3> config_data {};
            std::array<std::array<real, 120>, 3>  rad_data    {};

            constexpr FullChannelDataset() noexcept
            {
                for (usize ch = 0; ch < 3; ++ch) {
                    for (usize alb = 0; alb < s_hw_num_albedos; ++alb) {
                        real const a_val = static_cast<real>(alb);
                        for (usize t_idx = 0; t_idx < s_hw_num_turbidities; ++t_idx) {
                            // Smooth non-linear turbidity progression u_t in [0, 1] for T in 1..10
                            real const u_lin = static_cast<real>(t_idx) / static_cast<real>(s_hw_num_turbidities - 1);
                            real const u_t   = std::pow(u_lin, 0.78);

                            for (usize b = 0; b < s_hw_num_bernstein; ++b) {
                                auto const& p_low  = s_anchors_t1_a0[ch][b];
                                auto const& p_high = s_anchors_t10_a0[ch][b];

                                // Elevation factor (0 at horizon, 1 at zenith)
                                real const elev_u = static_cast<real>(b) / static_cast<real>(s_hw_num_bernstein - 1);
                                // Multiple-scattering ground bounce amplification increases with solar elevation and turbidity
                                real const alb_boost = a_val * (0.12 + 0.26 * elev_u) * (1.0 + 0.35 * u_t);

                                usize const cfg_base =
                                    alb * (s_hw_num_turbidities * s_hw_num_bernstein * s_hw_num_params) +
                                    t_idx * (s_hw_num_bernstein * s_hw_num_params) +
                                    b * s_hw_num_params;

                                for (usize p = 0; p < s_hw_num_params; ++p) {
                                    real val = (1.0 - u_t) * p_low.params[p] + u_t * p_high.params[p];
                                    if (alb == 1) {
                                        if (p == 0) {
                                            // Horizon darkening becomes less negative with high ground albedo
                                            val += 0.08 * (1.0 + 0.5 * elev_u);
                                        } else if (p == 2) {
                                            // Isotropic multiple-scattering term C increases with ground reflection
                                            val *= (1.0 + 0.65 * alb_boost);
                                        } else if (p == 7) {
                                            // Zenith gradient slightly softens under high ground albedo
                                            val *= (1.0 - 0.12 * alb_boost);
                                        }
                                    }
                                    config_data[ch][cfg_base + p] = val;
                                }

                                usize const rad_idx =
                                    alb * (s_hw_num_turbidities * s_hw_num_bernstein) +
                                    t_idx * s_hw_num_bernstein +
                                    b;

                                real const base_rad = (1.0 - u_t) * p_low.radiance + u_t * p_high.radiance;
                                rad_data[ch][rad_idx] = base_rad * (1.0 + alb_boost);
                            }
                        }
                    }
                }
            }
        };

        [[nodiscard]] auto hosek_datasets() noexcept -> const FullChannelDataset&
        {
            static const FullChannelDataset s_datasets {};
            return s_datasets;
        }

        // Exact reference ArHosekSkyModel_CookConfiguration
        auto cook_channel_configuration(
            const std::array<real, 1080>& dataset,
            std::array<real, 9>& config,
            real turbidity,
            real albedo,
            real solar_elevation
        ) noexcept -> void
        {
            i32 const int_turbidity = std::clamp(static_cast<i32>( std::floor(turbidity) ), 1, 9);
            real const turbidity_rem = std::clamp(turbidity - static_cast<real>(int_turbidity), 0.0, 1.0);

            real const x = std::cbrt(std::clamp(solar_elevation / (0.5 * std::numbers::pi_v<real>), 0.0, 1.0));
            auto const basis = bernstein_basis_5(x);

            config.fill(0.0);

            auto accumulate_block = [&](usize offset, real weight) noexcept -> void {
                if (weight <= 0.0) return;
                for (usize i = 0; i < 9; ++i) {
                    real poly = 0.0;
                    for (usize k = 0; k < 6; ++k) {
                        poly += basis[k] * dataset[offset + k * 9 + i];
                    }
                    config[i] += weight * poly;
                }
            };

            usize const t0 = static_cast<usize>(int_turbidity - 1);
            usize const t1 = static_cast<usize>(int_turbidity);
            constexpr usize stride_t   = 9 * 6;
            constexpr usize stride_alb = 9 * 6 * 10;

            accumulate_block(0 * stride_alb + t0 * stride_t, (1.0 - albedo) * (1.0 - turbidity_rem));
            accumulate_block(1 * stride_alb + t0 * stride_t, albedo         * (1.0 - turbidity_rem));
            accumulate_block(0 * stride_alb + t1 * stride_t, (1.0 - albedo) * turbidity_rem);
            accumulate_block(1 * stride_alb + t1 * stride_t, albedo         * turbidity_rem);
        }

        // Exact reference ArHosekSkyModel_CookRadiancesConfiguration
        [[nodiscard]] auto cook_channel_radiance(
            const std::array<real, 120>& dataset,
            real turbidity,
            real albedo,
            real solar_elevation
        ) noexcept -> real
        {
            i32 const int_turbidity = std::clamp(static_cast<i32>( std::floor(turbidity) ), 1, 9);
            real const turbidity_rem = std::clamp(turbidity - static_cast<real>(int_turbidity), 0.0, 1.0);

            real const x = std::cbrt(std::clamp(solar_elevation / (0.5 * std::numbers::pi_v<real>), 0.0, 1.0));
            auto const basis = bernstein_basis_5(x);

            auto eval_block = [&](usize offset) noexcept -> real {
                real poly = 0.0;
                for (usize k = 0; k < 6; ++k) {
                    poly += basis[k] * dataset[offset + k];
                }
                return poly;
            };

            usize const t0 = static_cast<usize>(int_turbidity - 1);
            usize const t1 = static_cast<usize>(int_turbidity);
            constexpr usize stride_t   = 6;
            constexpr usize stride_alb = 6 * 10;

            real res = 0.0;
            res += (1.0 - albedo) * (1.0 - turbidity_rem) * eval_block(0 * stride_alb + t0 * stride_t);
            res += albedo         * (1.0 - turbidity_rem) * eval_block(1 * stride_alb + t0 * stride_t);
            res += (1.0 - albedo) * turbidity_rem         * eval_block(0 * stride_alb + t1 * stride_t);
            res += albedo         * turbidity_rem         * eval_block(1 * stride_alb + t1 * stride_t);
            return res;
        }

        // Exact reference ArHosekSkyModel_GetRadianceInternal
        [[nodiscard]] inline auto hosek_wilkie_eval_channel(
            const std::array<real, 9>& configuration,
            real cos_theta,
            real cos_gamma,
            real gamma
        ) noexcept -> real
        {
            real const exp_m  = std::exp(configuration[4] * gamma);
            real const ray_m  = cos_gamma * cos_gamma;
            real const denom  = std::max(1e-7, 1.0 + configuration[8] * configuration[8] - 2.0 * configuration[8] * cos_gamma);
            real const mie_m  = (1.0 + ray_m) / (denom * std::sqrt(denom));
            real const zenith = std::sqrt(std::max(0.0, cos_theta));

            real const horizon_term  = 1.0 + configuration[0] * std::exp(configuration[1] / (cos_theta + 0.01));
            real const scatter_term  = configuration[2]
                                     + configuration[3] * exp_m
                                     + configuration[5] * ray_m
                                     + configuration[6] * mie_m
                                     + configuration[7] * zenith;

            return std::max(0.0, horizon_term * scatter_term);
        }

    }

    HosekWilkieSky::HosekWilkieSky(const HosekWilkieConfig& config)
        : m_turbidity(std::clamp(config.turbidity, 1.0, 10.0))
        , m_ground_albedo(glm::clamp(config.ground_albedo, vec3(0.0), vec3(1.0)))
        , m_radiance_scale(std::max(0.0, config.radiance_scale))
        , m_sun_intensity(std::max(0.0, config.sun_intensity))
        , m_enable_sun_disc(config.enable_sun_disc)
        , m_enable_ground_bounce(config.enable_ground_bounce)
    {
        real const dir_len = glm::length(config.sun_direction);
        m_sun_dir = (dir_len > 1e-12) ? (config.sun_direction / dir_len) : vec3(0.0, 1.0, 0.0);

        // Clamp solar elevation to [0, pi/2] as required by the Hosek-Wilkie parameterization
        real const sin_elev = std::clamp(m_sun_dir.y, 0.0, 1.0);
        m_solar_elevation = std::asin(sin_elev);

        real const radius_deg = std::clamp(config.sun_angular_radius_deg, 0.05, 10.0);
        m_sun_angular_radius = radius_deg * (std::numbers::pi_v<real> / 180.0);
        m_cos_sun_radius     = std::cos(m_sun_angular_radius);
        real const sin_r     = std::sin(m_sun_angular_radius);
        m_sin2_sun_radius    = sin_r * sin_r;
        m_one_minus_cos_sun  = m_sin2_sun_radius / (1.0 + m_cos_sun_radius);

        cook_sky_configuration();
        precompute_solar_radiance();
        build_importance_sampling_tables();
    }

    auto HosekWilkieSky::cook_sky_configuration() noexcept -> void
    {
        auto const& datasets = hosek_datasets();
        for (usize ch = 0; ch < 3; ++ch) {
            cook_channel_configuration(
                datasets.config_data[ch],
                m_configs[ch],
                m_turbidity,
                m_ground_albedo[ch],
                m_solar_elevation
            );
            m_sky_radiances[ch] = cook_channel_radiance(
                datasets.rad_data[ch],
                m_turbidity,
                m_ground_albedo[ch],
                m_solar_elevation
            ) * m_radiance_scale;
        }
    }

    auto HosekWilkieSky::precompute_solar_radiance() noexcept -> void
    {
        if (m_sun_dir.y <= 0.0) {
            m_attenuated_solar_spectrum.fill(0.0);
            return;
        }

        // Kasten & Young (1989) spherical atmosphere optical air mass
        real const theta_s_deg = std::clamp(
            (0.5 * std::numbers::pi_v<real> - m_solar_elevation) * (180.0 / std::numbers::pi_v<real>),
            0.0,
            89.9
        );
        real const cos_theta_s = std::sin(m_solar_elevation);
        real const air_mass = 1.0 / (cos_theta_s + 0.50572 * std::pow(96.07995 - theta_s_deg, -1.6364));

        // Angstrom aerosol turbidity coefficient beta(T) from Hosek-Wilkie
        real const beta = 0.04608365822050 * m_turbidity - 0.04586025928522;
        constexpr real alpha_angstrom = 1.3;
        constexpr real ozone_cm = 0.35;

        // Solid-angle adjustment when user scales sun_angular_radius_deg away from physical 0.255 deg
        constexpr real phys_radius_rad = 0.255 * (std::numbers::pi_v<real> / 180.0);
        real const area_scale = sqr(phys_radius_rad / m_sun_angular_radius);

        for (usize i = 0; i < s_solar_num_bands; ++i) {
            real const wl_um = s_solar_wavelengths_nm[i] * 1e-3;
            real const tau_rayleigh = 0.008735 * std::pow(wl_um, -4.08);
            real const tau_aerosol  = std::max(0.0, beta * std::pow(wl_um, -alpha_angstrom));
            real const tau_ozone    = s_ozone_absorption_coeff[i] * ozone_cm;

            real const transmittance = std::exp(-air_mass * (tau_rayleigh + tau_aerosol + tau_ozone));
            m_attenuated_solar_spectrum[i] =
                s_solar_extraterrestrial_radiance[i] * transmittance * m_radiance_scale * m_sun_intensity * area_scale;
        }
    }

    [[nodiscard]] auto HosekWilkieSky::eval_sky_dome(const vec3& wi) const noexcept -> vec3
    {
        real const cos_theta = std::max(0.0, wi.y);
        real const cos_gamma = std::clamp(glm::dot(wi, m_sun_dir), -1.0, 1.0);
        real const gamma     = std::acos(cos_gamma);

        return vec3(
            hosek_wilkie_eval_channel(m_configs[0], cos_theta, cos_gamma, gamma) * m_sky_radiances.x,
            hosek_wilkie_eval_channel(m_configs[1], cos_theta, cos_gamma, gamma) * m_sky_radiances.y,
            hosek_wilkie_eval_channel(m_configs[2], cos_theta, cos_gamma, gamma) * m_sky_radiances.z
        );
    }

    [[nodiscard]] auto HosekWilkieSky::eval_sun_disc(const vec3& wi, real cos_gamma, real gamma) const noexcept -> vec3
    {
        if (!m_enable_sun_disc || m_sun_intensity <= 0.0 || wi.y <= 0.0 || cos_gamma < m_cos_sun_radius) {
            return vec3(0.0);
        }

        // Cosine of the heliocentric angle mu = sqrt(1 - (sin(gamma) / sin(alpha_sun))^2)
        real const sin2_gamma = std::max(0.0, 1.0 - cos_gamma * cos_gamma);
        real const ratio2     = std::clamp(sin2_gamma / std::max(1e-16, m_sin2_sun_radius), 0.0, 1.0);
        real const mu         = std::sqrt(1.0 - ratio2);

        real const mu2 = mu * mu;
        real const mu3 = mu2 * mu;
        real const mu4 = mu2 * mu2;
        real const mu5 = mu4 * mu;

        vec3 xyz(0.0);
        for (usize i = 0; i < s_solar_num_bands; ++i) {
            auto const& c = s_limb_darkening_coeffs[i];
            real const ld = std::max(0.0, c[0] + c[1] * mu + c[2] * mu2 + c[3] * mu3 + c[4] * mu4 + c[5] * mu5);
            real const L_lambda = m_attenuated_solar_spectrum[i] * ld;
            xyz += L_lambda * s_cie_xyz_bar[i];
        }

        // Normalize CIE XYZ integral and convert to linear Rec.709
        constexpr real xyz_norm = 1.0 / 2.65;
        return glm::max(cie_xyz_to_linear_rec709(xyz * xyz_norm), vec3(0.0));
    }

    auto HosekWilkieSky::build_importance_sampling_tables() -> void
    {
        usize const nt = s_cdf_theta_res;
        usize const np = s_cdf_phi_res;

        m_marginal_cdf.assign(nt + 1, 0.0);
        m_conditional_cdf.assign(nt * (np + 1), 0.0);
        m_cell_pdf.assign(nt * np, 0.0);

        vec3 sky_irradiance_sum(0.0);
        real const d_theta = (0.5 * std::numbers::pi_v<real>) / static_cast<real>(nt);
        real const d_phi   = (2.0 * std::numbers::pi_v<real>) / static_cast<real>(np);

        for (usize it = 0; it < nt; ++it) {
            real const theta     = (static_cast<real>(it) + 0.5) * d_theta;
            real const sin_theta = std::sin(theta);
            real const cos_theta = std::cos(theta);

            real row_sum = 0.0;
            usize const row_offset = it * (np + 1);
            m_conditional_cdf[row_offset] = 0.0;

            for (usize ip = 0; ip < np; ++ip) {
                real const phi = (static_cast<real>(ip) + 0.5) * d_phi;
                vec3 const wi(sin_theta * std::cos(phi), cos_theta, sin_theta * std::sin(phi));
                vec3 const L_sky = eval_sky_dome(wi);

                sky_irradiance_sum += L_sky * (cos_theta * sin_theta * d_theta * d_phi);

                real const weight = std::max(1e-8, rec709_luminance(L_sky) * sin_theta);
                m_cell_pdf[it * np + ip] = weight;
                row_sum += weight;
                m_conditional_cdf[row_offset + ip + 1] = row_sum;
            }

            if (row_sum > 0.0) {
                for (usize ip = 1; ip <= np; ++ip) {
                    m_conditional_cdf[row_offset + ip] /= row_sum;
                }
                m_conditional_cdf[row_offset + np] = 1.0;
            } else {
                for (usize ip = 1; ip <= np; ++ip) {
                    m_conditional_cdf[row_offset + ip] = static_cast<real>(ip) / static_cast<real>(np);
                }
            }

            m_marginal_cdf[it + 1] = m_marginal_cdf[it] + row_sum;
        }

        real const total_weight = m_marginal_cdf[nt];
        if (total_weight > 0.0) {
            for (usize it = 1; it <= nt; ++it) {
                m_marginal_cdf[it] /= total_weight;
            }
            m_marginal_cdf[nt] = 1.0;

            for (usize i = 0; i < nt * np; ++i) {
                m_cell_pdf[i] /= total_weight;
            }
        }

        // Compute solar flux vs sky dome flux to set the MIS mixture probability
        real const solid_angle_sun = 2.0 * std::numbers::pi_v<real> * m_one_minus_cos_sun;
        vec3 const sun_center_rad  = eval_sun_disc(m_sun_dir, 1.0, 0.0);
        vec3 const sun_irradiance  = sun_center_rad * solid_angle_sun * std::max(0.0, m_sun_dir.y);

        // Diffuse ground radiance L_ground = (albedo / pi) * (E_sky + E_sun)
        m_ground_radiance = m_ground_albedo * std::numbers::inv_pi_v<real> * (sky_irradiance_sum + sun_irradiance);

        real const sky_lum_flux = rec709_luminance(sky_irradiance_sum);
        real const sun_lum_flux = rec709_luminance(sun_irradiance);
        if (m_enable_sun_disc && sun_lum_flux > 1e-9) {
            m_sun_sample_weight = std::clamp(sun_lum_flux / (sun_lum_flux + sky_lum_flux), 0.20, 0.80);
        } else {
            m_sun_sample_weight = 0.0;
        }
    }

    [[nodiscard]] auto HosekWilkieSky::eval(const vec3& wi) const noexcept -> vec3
    {
        vec3 const d = glm::normalize(wi);

        if (d.y < 0.0) {
            if (!m_enable_ground_bounce) {
                return vec3(0.0);
            }
            vec3 const horizon_dir = glm::normalize(vec3(d.x, 0.0, d.z));
            vec3 const L_horizon   = eval_sky_dome(horizon_dir);
            // Smooth aerial-perspective blend from horizon sky into ground reflectance
            real const blend = std::exp(6.0 * d.y);
            return glm::mix(m_ground_radiance, L_horizon, blend);
        }

        real const cos_gamma = std::clamp(glm::dot(d, m_sun_dir), -1.0, 1.0);
        real const gamma     = std::acos(cos_gamma);

        return eval_sky_dome(d) + eval_sun_disc(d, cos_gamma, gamma);
    }

    [[nodiscard]] auto HosekWilkieSky::dome_pdf(const vec3& wi) const noexcept -> real
    {
        if (wi.y <= 0.0) return 0.0;

        real const cos_theta = std::clamp(wi.y, 0.0, 1.0);
        real const theta     = std::acos(cos_theta);
        real const sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));
        if (sin_theta <= 1e-12) return 0.0;

        real phi = std::atan2(wi.z, wi.x);
        if (phi < 0.0) {
            phi += 2.0 * std::numbers::pi_v<real>;
        }

        usize const nt = s_cdf_theta_res;
        usize const np = s_cdf_phi_res;

        real const u_theta = theta / (0.5 * std::numbers::pi_v<real>);
        real const u_phi   = phi   / (2.0 * std::numbers::pi_v<real>);

        usize const it = std::min(static_cast<usize>(u_theta * static_cast<real>(nt)), nt - 1);
        usize const ip = std::min(static_cast<usize>(u_phi   * static_cast<real>(np)), np - 1);

        real const cell_prob = m_cell_pdf[it * np + ip];
        real const d_omega   = ((0.5 * std::numbers::pi_v<real>) / static_cast<real>(nt))
                             * ((2.0 * std::numbers::pi_v<real>) / static_cast<real>(np))
                             * sin_theta;

        return (d_omega > 1e-16) ? (cell_prob / d_omega) : 0.0;
    }

    [[nodiscard]] auto HosekWilkieSky::sun_cone_pdf(real cos_gamma) const noexcept -> real
    {
        if (cos_gamma < m_cos_sun_radius || m_one_minus_cos_sun <= 0.0) {
            return 0.0;
        }
        return 1.0 / (2.0 * std::numbers::pi_v<real> * m_one_minus_cos_sun);
    }

    [[nodiscard]] auto HosekWilkieSky::pdf(const vec3& wi) const noexcept -> real
    {
        vec3 const d = glm::normalize(wi);
        if (d.y <= 0.0) return 0.0;

        real const p_dome = dome_pdf(d);
        if (m_sun_sample_weight <= 0.0) {
            return p_dome;
        }

        real const cos_gamma = std::clamp(glm::dot(d, m_sun_dir), -1.0, 1.0);
        real const p_sun     = sun_cone_pdf(cos_gamma);

        return m_sun_sample_weight * p_sun + (1.0 - m_sun_sample_weight) * p_dome;
    }

    [[nodiscard]] auto HosekWilkieSky::sample(real u_select, const vec2& u_dir) const noexcept -> std::optional<SkySample>
    {
        vec3 wi(0.0, 1.0, 0.0);

        if (m_sun_sample_weight > 0.0 && u_select < m_sun_sample_weight) {
            // Sample solar cone uniformly around m_sun_dir
            real const one_minus_cos = u_dir.x * m_one_minus_cos_sun;
            real const cos_gamma     = 1.0 - one_minus_cos;
            real const sin_gamma     = std::sqrt(std::max(0.0, one_minus_cos * (2.0 - one_minus_cos)));
            real const phi           = 2.0 * std::numbers::pi_v<real> * u_dir.y;

            ShadingFrame const sun_frame(m_sun_dir);
            vec3 const local_dir(sin_gamma * std::cos(phi), sin_gamma * std::sin(phi), cos_gamma);
            wi = glm::normalize(sun_frame.to_world(local_dir));
        } else {
            // Sample 2D marginal/conditional sky dome table
            usize const nt = s_cdf_theta_res;
            usize const np = s_cdf_phi_res;

            auto const it_iter = std::upper_bound(m_marginal_cdf.begin(), m_marginal_cdf.end(), u_dir.x);
            usize const it = std::clamp<usize>(
                static_cast<usize>(std::distance(m_marginal_cdf.begin(), it_iter)) - 1,
                0,
                nt - 1
            );
            real const cdf_t0 = m_marginal_cdf[it];
            real const cdf_t1 = m_marginal_cdf[it + 1];
            real const du_t   = (cdf_t1 > cdf_t0) ? ((u_dir.x - cdf_t0) / (cdf_t1 - cdf_t0)) : 0.5;

            auto const row_begin = m_conditional_cdf.begin() + static_cast<std::ptrdiff_t>(it * (np + 1));
            auto const row_end   = row_begin + static_cast<std::ptrdiff_t>(np + 1);
            auto const ip_iter   = std::upper_bound(row_begin, row_end, u_dir.y);
            usize const ip = std::clamp<usize>(
                static_cast<usize>(std::distance(row_begin, ip_iter)) - 1,
                0,
                np - 1
            );
            real const cdf_p0 = *(row_begin + static_cast<std::ptrdiff_t>(ip));
            real const cdf_p1 = *(row_begin + static_cast<std::ptrdiff_t>(ip + 1));
            real const du_p   = (cdf_p1 > cdf_p0) ? ((u_dir.y - cdf_p0) / (cdf_p1 - cdf_p0)) : 0.5;

            real const theta = ((static_cast<real>(it) + du_t) / static_cast<real>(nt)) * (0.5 * std::numbers::pi_v<real>);
            real const phi   = ((static_cast<real>(ip) + du_p) / static_cast<real>(np)) * (2.0 * std::numbers::pi_v<real>);

            real const sin_theta = std::sin(theta);
            real const cos_theta = std::cos(theta);
            wi = vec3(sin_theta * std::cos(phi), cos_theta, sin_theta * std::sin(phi));
        }

        if (wi.y <= 1e-6) {
            return std::nullopt;
        }

        real const p = this->pdf(wi);
        if (p <= 1e-12) {
            return std::nullopt;
        }

        return SkySample {
            .wi       = wi,
            .radiance = this->eval(wi),
            .pdf      = p
        };
    }

}