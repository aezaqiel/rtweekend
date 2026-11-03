#include "principled.hpp"

#include <algorithm>
#include <array>
#include <numbers>

namespace rtweekend {

    namespace {

        inline constexpr real s_min_alpha = 1e-4;

        [[nodiscard]] constexpr auto sqr(real x) noexcept -> real
        {
            return x * x;
        }

        [[nodiscard]] constexpr auto schlick_weight(real u) noexcept -> real
        {
            real const m = std::clamp(1.0 - u, 0.0, 1.0);
            real const m2 = m * m;
            return m2 * m2 * m;
        }

        [[nodiscard]] constexpr auto rec709_luminance(const vec3& c) noexcept -> real
        {
            return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z;
        }

        // Exact Maxwell unpolarized Fresnel reflectance for a dielectric interface (eta = eta_i / eta_t)
        [[nodiscard]] inline auto fresnel_dielectric(real cos_theta_i, real eta) noexcept -> real
        {
            cos_theta_i = std::clamp(std::abs(cos_theta_i), 0.0, 1.0);
            real const sin2_theta_i = std::max(0.0, 1.0 - cos_theta_i * cos_theta_i);
            real const sin2_theta_t = eta * eta * sin2_theta_i;
            if (sin2_theta_t >= 1.0) {
                return 1.0;
            }

            real const cos_theta_t = std::sqrt(std::max(0.0, 1.0 - sin2_theta_t));
            real const r_perp = (eta * cos_theta_i - cos_theta_t) / (eta * cos_theta_i + cos_theta_t);
            real const r_parl = (cos_theta_i - eta * cos_theta_t) / (cos_theta_i + eta * cos_theta_t);
            return 0.5 * (r_perp * r_perp + r_parl * r_parl);
        }

        // Incoherent dual-interface Airy reflectance for a thin dielectric sheet
        [[nodiscard]] inline auto fresnel_thin_dielectric(real cos_theta_i, real eta) noexcept -> real
        {
            real const f1 = fresnel_dielectric(cos_theta_i, eta);
            return (2.0 * f1) / (1.0 + f1);
        }

        // Exact Maxwell unpolarized Fresnel reflectance for a complex conductor (eta + i*k)
        [[nodiscard]] inline auto fresnel_conductor_channel(real cos_theta_i, real eta, real k) noexcept -> real
        {
            cos_theta_i = std::clamp(std::abs(cos_theta_i), 0.0, 1.0);
            if (cos_theta_i <= 0.0) return 1.0;

            real const cos2 = cos_theta_i * cos_theta_i;
            real const sin2 = std::max(0.0, 1.0 - cos2);
            real const eta2 = eta * eta;
            real const k2   = k * k;

            real const t0 = eta2 - k2 - sin2;
            real const a2plusb2 = std::sqrt(std::max(0.0, t0 * t0 + 4.0 * eta2 * k2));
            real const t1 = a2plusb2 + cos2;
            real const a  = std::sqrt(std::max(0.0, 0.5 * (a2plusb2 + t0)));
            real const t2 = 2.0 * cos_theta_i * a;
            real const rs = (t1 - t2) / std::max(1e-16, t1 + t2);

            real const t3 = cos2 * a2plusb2 + sin2 * sin2;
            real const t4 = t2 * sin2;
            real const rp = rs * (t3 - t4) / std::max(1e-16, t3 + t4);

            return std::clamp(0.5 * (rs + rp), 0.0, 1.0);
        }

        [[nodiscard]] inline auto fresnel_conductor(real cos_theta_i, const vec3& eta, const vec3& k) noexcept -> vec3
        {
            return vec3(
                fresnel_conductor_channel(cos_theta_i, eta.x, k.x),
                fresnel_conductor_channel(cos_theta_i, eta.y, k.y),
                fresnel_conductor_channel(cos_theta_i, eta.z, k.z)
            );
        }

        // Hemispherical average Fresnel reflectance F_avg = 2 * \int_0^1 F(mu) mu dmu
        // via 8-point Gauss-Legendre quadrature
        [[nodiscard]] inline auto average_fresnel_conductor(const vec3& eta, const vec3& k) noexcept -> vec3
        {
            static constexpr real s_nodes[4] = {
                0.1834346424956498, 0.5255324099163290, 0.7966664774136267, 0.9602898564975363
            };
            static constexpr real s_weights[4] = {
                0.3626837833783620, 0.3137066458778873, 0.2223810344533745, 0.1012285362903763
            };

            vec3 sum(0.0);
            for (int i = 0; i < 4; ++i) {
                for (int sign : { -1, 1 }) {
                    real const mu = 0.5 + 0.5 * sign * s_nodes[i];
                    real const w  = 0.5 * s_weights[i];
                    sum += w * (2.0 * mu) * fresnel_conductor(mu, eta, k);
                }
            }
            return glm::clamp(sum, vec3(0.0), vec3(0.999));
        }

        // Exact analytical hemispherical average Fresnel for a dielectric (Kulla & Conty 2017)
        [[nodiscard]] inline auto average_fresnel_dielectric(real ior) noexcept -> real
        {
            if (ior <= 1.0) return 0.0;
            return (ior - 1.0) / (4.08567 + 1.00071 * ior);
        }

        // Gulbrandsen (2014) exact analytical inversion from normal reflectance r and edge tint g
        // to complex index of refraction (eta, k).
        inline auto gulbrandsen_ior_from_artistic(const vec3& r_in, const vec3& g_in, vec3& out_eta, vec3& out_k) noexcept -> void
        {
            for (int c = 0; c < 3; ++c) {
                real const r = std::clamp(r_in[c], 0.0, 0.999999);
                real const g = std::clamp(g_in[c], 0.0, 1.0);
                real const sqrt_r = std::sqrt(r);

                real const n_min = (1.0 - r) / (1.0 + r);
                real const n_max = (1.0 + sqrt_r) / (1.0 - sqrt_r);
                real const eta = g * n_min + (1.0 - g) * n_max;
                real const k2 = (sqr(eta + 1.0) * r - sqr(eta - 1.0)) / (1.0 - r);

                out_eta[c] = eta;
                out_k[c]   = std::sqrt(std::max(0.0, k2));
            }
        }

        [[nodiscard]] inline auto d_ggx_aniso(const vec3& h, real ax, real ay) noexcept -> real
        {
            if (h.z <= 0.0) return 0.0;
            real const term = sqr(h.x / ax) + sqr(h.y / ay) + sqr(h.z);
            return 1.0 / (std::numbers::pi_v<real> * ax * ay * sqr(term));
        }

        [[nodiscard]] inline auto lambda_smith_ggx_aniso(const vec3& v, real ax, real ay) noexcept -> real
        {
            if (v.z == 0.0) return 0.0;
            real const tan2_theta = (sqr(v.x * ax) + sqr(v.y * ay)) / sqr(v.z);
            return 0.5 * (-1.0 + std::sqrt(1.0 + tan2_theta));
        }

        [[nodiscard]] inline auto g1_smith_ggx_aniso(const vec3& v, real ax, real ay) noexcept -> real
        {
            if (v.z == 0.0) return 0.0;
            return 1.0 / (1.0 + lambda_smith_ggx_aniso(v, ax, ay));
        }

        [[nodiscard]] inline auto g2_smith_ggx_aniso(const vec3& wo, const vec3& wi, real ax, real ay) noexcept -> real
        {
            if (wo.z == 0.0 || wi.z == 0.0) return 0.0;
            return 1.0 / (1.0 + lambda_smith_ggx_aniso(wo, ax, ay) + lambda_smith_ggx_aniso(wi, ax, ay));
        }

        // Precomputed 32x32 lookup table for single-scattering GGX directional albedo E(cos_theta, alpha)
        // and hemispherical average albedo E_avg(alpha) = 2 * \int_0^1 E(mu, alpha) mu dmu
        struct GGXAlbedoLUT
        {
            static constexpr usize s_res = 32;
            std::array<real, s_res * s_res> e_dir {};
            std::array<real, s_res>         e_avg {};

            GGXAlbedoLUT() noexcept
            {
                static constexpr real s_nodes[4] = {
                    0.1834346424956498, 0.5255324099163290, 0.7966664774136267, 0.9602898564975363
                };
                static constexpr real s_weights[4] = {
                    0.3626837833783620, 0.3137066458778873, 0.2223810344533745, 0.1012285362903763
                };

                auto integrate_e = [](real mu, real alpha) noexcept -> real {
                    mu    = std::clamp(mu, 1e-4, 1.0);
                    alpha = std::clamp(alpha, s_min_alpha, 1.0);

                    vec3 const wo(std::sqrt(std::max(0.0, 1.0 - mu * mu)), 0.0, mu);
                    vec3 const vh = glm::normalize(vec3(alpha * wo.x, 0.0, wo.z));
                    vec3 const t1(0.0, 1.0, 0.0);
                    vec3 const t2 = glm::cross(vh, t1);
                    real const s = 0.5 * (1.0 + vh.z);
                    real const lambda_o = lambda_smith_ggx_aniso(wo, alpha, alpha);

                    real integral = 0.0;
                    for (int i = 0; i < 4; ++i) {
                        for (int sign_r : { -1, 1 }) {
                            real const u_r = 0.5 + 0.5 * sign_r * s_nodes[i];
                            real const r   = std::sqrt(u_r);
                            real const wr  = 0.5 * s_weights[i];
                            for (int j = 0; j < 4; ++j) {
                                for (int sign_p : { -1, 1 }) {
                                    real const u_p = 0.5 + 0.5 * sign_p * s_nodes[j];
                                    real const phi = 2.0 * std::numbers::pi_v<real> * u_p;
                                    real const wp  = 0.5 * s_weights[j];

                                    real const px = r * std::cos(phi);
                                    real py = r * std::sin(phi);
                                    py = (1.0 - s) * std::sqrt(std::max(0.0, 1.0 - px * px)) + s * py;
                                    real const pz = std::sqrt(std::max(0.0, 1.0 - px * px - py * py));

                                    vec3 const nh = px * t1 + py * t2 + pz * vh;
                                    vec3 const h  = glm::normalize(vec3(alpha * nh.x, alpha * nh.y, std::max(1e-6, nh.z)));
                                    vec3 const wi = glm::reflect(-wo, h);
                                    if (wi.z > 0.0) {
                                        real const lambda_i = lambda_smith_ggx_aniso(wi, alpha, alpha);
                                        integral += wr * wp * ((1.0 + lambda_o) / (1.0 + lambda_o + lambda_i));
                                    }
                                }
                            }
                        }
                    }
                    return std::clamp(integral, 1e-4, 1.0);
                };

                for (usize ia = 0; ia < s_res; ++ia) {
                    real const alpha = std::max(s_min_alpha, static_cast<real>(ia) / static_cast<real>(s_res - 1));
                    for (usize im = 0; im < s_res; ++im) {
                        real const mu = std::max(1e-4, static_cast<real>(im) / static_cast<real>(s_res - 1));
                        e_dir[ia * s_res + im] = integrate_e(mu, alpha);
                    }

                    real avg_sum = 0.0;
                    for (int i = 0; i < 4; ++i) {
                        for (int sign : { -1, 1 }) {
                            real const mu = 0.5 + 0.5 * sign * s_nodes[i];
                            real const w  = 0.5 * s_weights[i];
                            avg_sum += w * (2.0 * mu) * integrate_e(mu, alpha);
                        }
                    }
                    e_avg[ia] = std::clamp(avg_sum, 1e-4, 1.0);
                }
            }

            [[nodiscard]] auto directional(real cos_theta, real alpha) const noexcept -> real
            {
                real const u_m = std::clamp(cos_theta, 0.0, 1.0) * static_cast<real>(s_res - 1);
                real const u_a = std::clamp(alpha, 0.0, 1.0) * static_cast<real>(s_res - 1);

                usize const m0 = std::min(static_cast<usize>(u_m), s_res - 2);
                usize const a0 = std::min(static_cast<usize>(u_a), s_res - 2);
                real const tm = u_m - static_cast<real>(m0);
                real const ta = u_a - static_cast<real>(a0);

                real const e00 = e_dir[a0 * s_res + m0];
                real const e10 = e_dir[a0 * s_res + (m0 + 1)];
                real const e01 = e_dir[(a0 + 1) * s_res + m0];
                real const e11 = e_dir[(a0 + 1) * s_res + (m0 + 1)];

                real const e0 = (1.0 - tm) * e00 + tm * e10;
                real const e1 = (1.0 - tm) * e01 + tm * e11;
                return (1.0 - ta) * e0 + ta * e1;
            }

            [[nodiscard]] auto average(real alpha) const noexcept -> real
            {
                real const u_a = std::clamp(alpha, 0.0, 1.0) * static_cast<real>(s_res - 1);
                usize const a0 = std::min(static_cast<usize>(u_a), s_res - 2);
                real const ta = u_a - static_cast<real>(a0);
                return (1.0 - ta) * e_avg[a0] + ta * e_avg[a0 + 1];
            }
        };

        [[nodiscard]] auto ggx_lut() noexcept -> const GGXAlbedoLUT&
        {
            static const GGXAlbedoLUT s_lut {};
            return s_lut;
        }

        [[nodiscard]] inline auto sample_ggx_vndf(const vec3& wo, real ax, real ay, const vec2& u) noexcept -> vec3
        {
            vec3 const vh = glm::normalize(vec3(ax * wo.x, ay * wo.y, wo.z));
            real const lensq = vh.x * vh.x + vh.y * vh.y;
            vec3 const t1 = lensq > 1e-12
                ? vec3(-vh.y, vh.x, 0.0) / std::sqrt(lensq)
                : vec3(1.0, 0.0, 0.0);
            vec3 const t2 = glm::cross(vh, t1);

            vec2 p = warp::concentric_unit_disk(u);
            real const s = 0.5 * (1.0 + vh.z);
            p.y = (1.0 - s) * std::sqrt(std::max(0.0, 1.0 - p.x * p.x)) + s * p.y;
            real const pz = std::sqrt(std::max(0.0, 1.0 - p.x * p.x - p.y * p.y));

            vec3 const nh = p.x * t1 + p.y * t2 + pz * vh;
            return glm::normalize(vec3(ax * nh.x, ay * nh.y, std::max(1e-6, nh.z)));
        }

        // Estevez & Kulla (2017) "Charlie" microfiber sheen distribution & visibility
        [[nodiscard]] inline auto d_charlie(real cos_theta_h, real alpha) noexcept -> real
        {
            if (cos_theta_h <= 0.0) return 0.0;
            real const inv_a = 1.0 / std::max(1e-4, alpha);
            real const sin2 = std::max(0.0, 1.0 - cos_theta_h * cos_theta_h);
            real const sin_theta = std::sqrt(sin2);
            return (2.0 + inv_a) * std::pow(sin_theta, inv_a) * (0.5 * std::numbers::inv_pi_v<real>);
        }

        [[nodiscard]] inline auto v_ashikhmin(real cos_theta_o, real cos_theta_i) noexcept -> real
        {
            real const denom = 4.0 * (cos_theta_o + cos_theta_i - cos_theta_o * cos_theta_i);
            return (denom > 1e-12) ? (1.0 / denom) : 0.0;
        }

        // Analytical directional albedo fit for Charlie microfiber sheen
        [[nodiscard]] constexpr auto sheen_directional_albedo(real cos_theta, real alpha) noexcept -> real
        {
            real const c = 1.0 - std::clamp(cos_theta, 0.0, 1.0);
            real const c3 = c * c * c;
            return std::clamp(0.065 * alpha + (0.42 + 0.35 * (1.0 - alpha)) * c3, 0.0, 1.0);
        }

        [[nodiscard]] inline auto d_gtr1(const vec3& h, real alpha) noexcept -> real
        {
            if (h.z <= 0.0) return 0.0;
            if (alpha >= 1.0) return std::numbers::inv_pi_v<real>;
            real const a2 = alpha * alpha;
            real const denom = 1.0 + (a2 - 1.0) * h.z * h.z;
            return (a2 - 1.0) / (std::numbers::pi_v<real> * std::log(a2) * denom);
        }

        [[nodiscard]] inline auto lambda_smith_gtr1(const vec3& v, real alpha) noexcept -> real
        {
            if (v.z <= 0.0) return 0.0;
            real const cos2 = std::min(1.0, v.z * v.z);
            real const sin2 = std::max(0.0, 1.0 - cos2);
            if (sin2 <= 1e-12) return 0.0;

            real const tan2 = sin2 / cos2;
            real const tan_theta = std::sqrt(tan2);
            real const a = std::clamp(alpha, 1e-4, 0.9999);
            real const a2 = a * a;

            real const s_a = std::sqrt(1.0 + a2 * tan2);
            real const s_1 = std::sqrt(1.0 + tan2);
            real const log_term = std::log((tan_theta * (1.0 + s_a)) / (a * (1.0 + s_1)));
            return std::max(0.0, (s_a - s_1 + log_term) / (2.0 * std::log(a)));
        }

        [[nodiscard]] inline auto g2_smith_gtr1(const vec3& wo, const vec3& wi, real alpha) noexcept -> real
        {
            if (wo.z <= 0.0 || wi.z <= 0.0) return 0.0;
            return 1.0 / (1.0 + lambda_smith_gtr1(wo, alpha) + lambda_smith_gtr1(wi, alpha));
        }

        [[nodiscard]] inline auto sample_gtr1(real alpha, const vec2& u) noexcept -> vec3
        {
            real cos_theta = 1.0;
            if (alpha < 1.0) {
                real const a2 = alpha * alpha;
                real const cos2_theta = (1.0 - std::pow(a2, 1.0 - u.x)) / (1.0 - a2);
                cos_theta = std::sqrt(std::clamp(cos2_theta, 0.0, 1.0));
            } else {
                cos_theta = std::sqrt(1.0 - u.x);
            }
            real const sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));
            real const phi = 2.0 * std::numbers::pi_v<real> * u.y;
            return vec3(sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta);
        }

    }

    PrincipledBSDF::PrincipledBSDF(const PrincipledBSDFConfig& config)
        : m_base_color(glm::clamp(config.base_color, vec3(0.0), vec3(1.0)))
        , m_metallic(std::clamp(config.metallic, 0.0, 1.0))
        , m_roughness(std::clamp(config.roughness, 0.0, 1.0))
        , m_subsurface(std::clamp(config.subsurface, 0.0, 1.0))
        , m_specular(std::max(0.0, config.specular))
        , m_specular_tint(std::clamp(config.specular_tint, 0.0, 1.0))
        , m_anisotropic(std::clamp(config.anisotropic, 0.0, 1.0))
        , m_sheen(std::max(0.0, config.sheen))
        , m_sheen_tint(std::clamp(config.sheen_tint, 0.0, 1.0))
        , m_sheen_roughness(std::clamp(config.sheen_roughness, 0.05, 1.0))
        , m_clearcoat(std::max(0.0, config.clearcoat))
        , m_clearcoat_roughness(std::clamp(config.clearcoat_roughness, 0.0, 1.0))
        , m_clearcoat_ior(std::max(1.001, config.clearcoat_ior))
        , m_clearcoat_tint(glm::clamp(config.clearcoat_tint, vec3(1e-4), vec3(1.0)))
        , m_ior(std::max(1.001, config.ior))
        , m_spec_trans(std::clamp(config.spec_trans, 0.0, 1.0))
        , m_thin_walled(config.thin_walled)
        , m_emission(glm::max(config.emission, vec3(0.0)))
    {
        m_is_emissive = (m_emission.x > 0.0 || m_emission.y > 0.0 || m_emission.z > 0.0);

        real const aspect = std::sqrt(1.0 - 0.9 * m_anisotropic);
        real const r2 = m_roughness * m_roughness;
        m_is_delta_mf = (r2 <= 1e-7);

        m_alpha_x = std::max(s_min_alpha, r2 / aspect);
        m_alpha_y = std::max(s_min_alpha, r2 * aspect);
        m_alpha_iso = std::clamp(std::sqrt(0.5 * (sqr(m_alpha_x) + sqr(m_alpha_y))), s_min_alpha, 1.0);
        m_alpha_coat = std::max(0.001, sqr(m_clearcoat_roughness));
        m_alpha_sheen = std::clamp(sqr(m_sheen_roughness), 0.0025, 1.0);

        real const dielec_diffuse_weight = (1.0 - m_metallic) * (1.0 - m_spec_trans);
        bool const has_diffuse_or_sheen = (dielec_diffuse_weight > 1e-6) &&
            (rec709_luminance(m_base_color) > 1e-6 || m_sheen > 1e-6);
        m_has_non_delta = (!m_is_delta_mf) || has_diffuse_or_sheen || (m_clearcoat > 1e-6);

        real const lum = rec709_luminance(m_base_color);
        m_tint = (lum > 1e-6) ? (m_base_color / lum) : vec3(1.0);
        m_spec_tint_color = glm::mix(vec3(1.0), m_tint, m_specular_tint);
        m_sheen_color = glm::mix(vec3(1.0), m_tint, m_sheen_tint);

        real const f0_spec = std::clamp(0.08 * m_specular, 0.0, 0.99);
        m_opaque_ior = (2.0 / (1.0 - std::sqrt(f0_spec))) - 1.0;

        gulbrandsen_ior_from_artistic(m_base_color, m_spec_tint_color, m_conductor_eta, m_conductor_k);
        m_f_avg_metal  = average_fresnel_conductor(m_conductor_eta, m_conductor_k);
        m_f_avg_opaque = (m_specular > 0.0) ? average_fresnel_dielectric(m_opaque_ior) : 0.0;

        // Precompute reciprocal Kulla-Conty (2017) multiscattering energy factor
        real const e_avg = ggx_lut().average(m_alpha_iso);
        real const one_minus_e_avg = std::max(1e-6, 1.0 - e_avg);
        vec3 const f_ms_metal = (m_f_avg_metal * m_f_avg_metal * e_avg) /
            glm::max(vec3(1e-6), vec3(1.0) - m_f_avg_metal * (1.0 - e_avg));
        real const f_ms_opaque = (sqr(m_f_avg_opaque) * e_avg) /
            std::max(1e-6, 1.0 - m_f_avg_opaque * (1.0 - e_avg));
        vec3 const f_ms_blend = glm::mix(
            vec3(f_ms_opaque * (1.0 - m_spec_trans)),
            f_ms_metal,
            m_metallic
        );
        m_ms_energy_factor = f_ms_blend / (std::numbers::pi_v<real> * one_minus_e_avg);

        m_coat_sigma_a = -glm::log(m_clearcoat_tint);
        m_thin_trans_color = glm::sqrt(glm::clamp(m_base_color, vec3(0.0), vec3(1.0)));

        if (!m_thin_walled && m_spec_trans > 0.0 && m_metallic < 1.0) {
            real const atten_dist = std::max(1e-6, config.attenuation_distance);
            vec3 const clamped_color = glm::clamp(m_base_color, vec3(1e-6), vec3(1.0));
            m_sigma_a = -glm::log(clamped_color) / atten_dist;
            m_has_medium = (m_sigma_a.x > 1e-7 || m_sigma_a.y > 1e-7 || m_sigma_a.z > 1e-7);
        }
    }

    auto PrincipledBSDF::diffuse(const vec3& base_color, real roughness, real subsurface) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
            .base_color = base_color,
            .metallic   = 0.0,
            .roughness  = roughness,
            .subsurface = subsurface,
            .specular   = 0.0
        });
    }

    auto PrincipledBSDF::metal(const vec3& base_color, real roughness, real anisotropic) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
            .base_color  = base_color,
            .metallic    = 1.0,
            .roughness   = roughness,
            .anisotropic = anisotropic
        });
    }

    auto PrincipledBSDF::dielectric(real ior, real roughness, const vec3& base_color, real attenuation_distance) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
            .base_color           = base_color,
            .metallic             = 0.0,
            .roughness            = roughness,
            .ior                  = ior,
            .spec_trans           = 1.0,
            .attenuation_distance = attenuation_distance,
            .thin_walled          = false
        });
    }

    auto PrincipledBSDF::thin_dielectric(real ior, real roughness, const vec3& base_color) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
            .base_color  = base_color,
            .metallic    = 0.0,
            .roughness   = roughness,
            .ior         = ior,
            .spec_trans  = 1.0,
            .thin_walled = true
        });
    }

    auto PrincipledBSDF::plastic(const vec3& base_color, real roughness, real clearcoat) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
            .base_color          = base_color,
            .metallic            = 0.0,
            .roughness           = roughness,
            .specular            = 0.5,
            .clearcoat           = clearcoat,
            .clearcoat_roughness = 0.03
        });
    }

    auto PrincipledBSDF::fabric(const vec3& base_color, real roughness, real sheen, real sheen_tint) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
            .base_color      = base_color,
            .metallic        = 0.0,
            .roughness       = roughness,
            .specular        = 0.15,
            .sheen           = sheen,
            .sheen_tint      = sheen_tint,
            .sheen_roughness = 0.35
        });
    }

    auto PrincipledBSDF::emissive(const vec3& radiance) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
            .base_color = vec3(0.0),
            .specular   = 0.0,
            .emission   = radiance
        });
    }

    auto PrincipledBSDF::effective_alpha(const vec3& v) const noexcept -> real
    {
        real const xy2 = v.x * v.x + v.y * v.y;
        if (xy2 <= 1e-12) return m_alpha_iso;
        return std::sqrt((sqr(v.x * m_alpha_x) + sqr(v.y * m_alpha_y)) / xy2);
    }

    auto PrincipledBSDF::coat_transmission(real cos_theta_o, real cos_theta_i) const noexcept -> vec3
    {
        if (m_clearcoat <= 0.0) return vec3(1.0);
        real const fc_o = fresnel_dielectric(cos_theta_o, 1.0 / m_clearcoat_ior);
        real const fc_i = fresnel_dielectric(cos_theta_i, 1.0 / m_clearcoat_ior);
        real const t_fresnel = (1.0 - 0.25 * m_clearcoat * fc_o) * (1.0 - 0.25 * m_clearcoat * fc_i);

        if (m_coat_sigma_a.x <= 1e-6 && m_coat_sigma_a.y <= 1e-6 && m_coat_sigma_a.z <= 1e-6) {
            return vec3(t_fresnel);
        }

        real const sin2_o = std::max(0.0, 1.0 - cos_theta_o * cos_theta_o) / sqr(m_clearcoat_ior);
        real const sin2_i = std::max(0.0, 1.0 - cos_theta_i * cos_theta_i) / sqr(m_clearcoat_ior);
        real const mu_o = std::sqrt(std::max(1e-4, 1.0 - sin2_o));
        real const mu_i = std::sqrt(std::max(1e-4, 1.0 - sin2_i));
        real const path_len = m_clearcoat * (1.0 / mu_o + 1.0 / mu_i);
        return t_fresnel * glm::exp(-m_coat_sigma_a * path_len);
    }

    auto PrincipledBSDF::compute_lobe_weights(real cos_theta_o, real eta, bool is_interior) const noexcept -> LobeWeights
    {
        if (is_interior) {
            return LobeWeights {
                .diffuse    = 0.0,
                .microfacet = 1.0,
                .clearcoat  = 0.0
            };
        }

        real const f_coat = (m_clearcoat > 0.0) ? fresnel_dielectric(cos_theta_o, 1.0 / m_clearcoat_ior) : 0.0;
        real const coat_trans = std::max(0.0, 1.0 - 0.25 * m_clearcoat * f_coat);

        real const dielec_weight = (1.0 - m_metallic) * (1.0 - m_spec_trans);
        real const base_lum = rec709_luminance(m_base_color);
        real const sheen_lum = m_sheen * rec709_luminance(m_sheen_color) * sheen_directional_albedo(cos_theta_o, m_alpha_sheen);
        real const f_opaque = (m_specular > 0.0) ? fresnel_dielectric(cos_theta_o, 1.0 / m_opaque_ior) : 0.0;
        real const diff_lum = (1.0 - f_opaque) * base_lum + sheen_lum;
        real const w_diff = (dielec_weight > 0.0) ? (coat_trans * dielec_weight * std::max(0.02, diff_lum)) : 0.0;

        real const f_diel = m_thin_walled
            ? fresnel_thin_dielectric(cos_theta_o, eta)
            : fresnel_dielectric(cos_theta_o, eta);
        real const f_metal = rec709_luminance(fresnel_conductor(cos_theta_o, m_conductor_eta, m_conductor_k));

        real const f_spec_est = glm::mix(glm::mix(f_opaque, f_diel, m_spec_trans), std::max(0.05, f_metal), m_metallic);
        real const t_est = (1.0 - m_metallic) * m_spec_trans * (1.0 - f_diel);
        real const w_mf = coat_trans * std::max(0.0, f_spec_est + t_est);

        real const w_coat = 0.25 * m_clearcoat * f_coat;
        real const sum = w_diff + w_mf + w_coat;
        if (sum <= 1e-12) {
            return LobeWeights { 1.0, 0.0, 0.0 };
        }

        return LobeWeights {
            .diffuse    = w_diff / sum,
            .microfacet = w_mf / sum,
            .clearcoat  = w_coat / sum
        };
    }

    auto PrincipledBSDF::transmission_prob(real cos_theta_d, real eta, bool is_interior) const noexcept -> real
    {
        real const trans_scale = (1.0 - m_metallic) * m_spec_trans;
        if (trans_scale <= 0.0) {
            return 0.0;
        }

        real const f_diel = m_thin_walled
            ? fresnel_thin_dielectric(cos_theta_d, eta)
            : fresnel_dielectric(cos_theta_d, eta);

        if (is_interior) {
            return 1.0 - f_diel;
        }

        real const t_weight = trans_scale * (1.0 - f_diel);
        real const r_weight = glm::mix(f_diel, 1.0, m_metallic);
        real const denom = t_weight + r_weight;
        return (denom > 1e-12) ? (t_weight / denom) : 0.0;
    }

    auto PrincipledBSDF::specular_fresnel(real cos_theta_d, real eta, bool is_interior) const noexcept -> vec3
    {
        real const f_diel = m_thin_walled
            ? fresnel_thin_dielectric(cos_theta_d, eta)
            : fresnel_dielectric(cos_theta_d, eta);

        if (is_interior) {
            return vec3(f_diel);
        }

        real const f_opaque_scalar = (m_specular > 0.0) ? fresnel_dielectric(cos_theta_d, 1.0 / m_opaque_ior) : 0.0;
        vec3 const f_opaque = f_opaque_scalar * glm::mix(m_spec_tint_color, vec3(1.0), f_opaque_scalar);
        vec3 const f_dielectric_blend = glm::mix(f_opaque, f_diel * m_spec_tint_color, m_spec_trans);
        vec3 const f_metal = fresnel_conductor(cos_theta_d, m_conductor_eta, m_conductor_k);
        return glm::mix(f_dielectric_blend, f_metal, m_metallic);
    }

    auto PrincipledBSDF::eval_local(const vec3& wo, const vec3& wi, real eta, bool is_interior) const noexcept -> vec3
    {
        if (wo.z <= 0.0 || std::abs(wi.z) <= 1e-8) {
            return vec3(0.0);
        }

        // Continuous evaluation of transmission (wi.z < 0)
        if (wi.z < 0.0) {
            if (m_is_delta_mf) {
                return vec3(0.0);
            }
            real const trans_scale = (1.0 - m_metallic) * m_spec_trans;
            if (trans_scale <= 0.0) {
                return vec3(0.0);
            }

            if (m_thin_walled) {
                vec3 const wi_refl(wi.x, wi.y, -wi.z);
                vec3 h = wo + wi_refl;
                real const len2 = glm::dot(h, h);
                if (len2 <= 1e-16) return vec3(0.0);
                h /= std::sqrt(len2);

                real const wo_dot_h = std::max(0.0, glm::dot(wo, h));
                real const f_thin = fresnel_thin_dielectric(wo_dot_h, eta);
                real const d = d_ggx_aniso(h, m_alpha_x, m_alpha_y);
                real const g = g2_smith_ggx_aniso(wo, wi_refl, m_alpha_x, m_alpha_y);
                vec3 const coat_atten = coat_transmission(wo.z, wi_refl.z);

                return coat_atten * m_thin_trans_color * (trans_scale * (1.0 - f_thin) * d * g / (4.0 * wo.z * wi_refl.z));
            }

            // Physical Snell refraction half-vector points along -(eta * wo + wi) when eta < 1,
            // and +(eta * wo + wi) when eta > 1.
            vec3 h = (eta < 1.0) ? -(eta * wo + wi) : (eta * wo + wi);
            real const len2 = glm::dot(h, h);
            if (len2 <= 1e-16) {
                return vec3(0.0);
            }
            h /= std::sqrt(len2);
            if (h.z <= 0.0) {
                return vec3(0.0);
            }

            real const wo_dot_h = glm::dot(wo, h);
            real const wi_dot_h = glm::dot(wi, h);
            if (wo_dot_h <= 0.0 || wi_dot_h >= 0.0) {
                return vec3(0.0);
            }

            real const f_diel = fresnel_dielectric(wo_dot_h, eta);
            if (f_diel >= 1.0) {
                return vec3(0.0);
            }

            real const d = d_ggx_aniso(h, m_alpha_x, m_alpha_y);
            real const g = g2_smith_ggx_aniso(wo, vec3(wi.x, wi.y, -wi.z), m_alpha_x, m_alpha_y);
            real const denom = sqr(eta * wo_dot_h + wi_dot_h) * wo.z * std::abs(wi.z);
            if (denom <= 1e-16) {
                return vec3(0.0);
            }

            // Includes exact Clausius (eta^2) non-symmetric radiance scaling
            real const factor = (sqr(eta) * d * g * (1.0 - f_diel) * wo_dot_h * std::abs(wi_dot_h)) / denom;
            vec3 const coat_atten = is_interior ? vec3(1.0) : coat_transmission(wo.z, std::abs(wi.z));
            return coat_atten * (trans_scale * factor);
        }

        // Reflection hemisphere (wi.z > 0)
        vec3 h = wo + wi;
        real const len2 = glm::dot(h, h);
        if (len2 <= 1e-16) {
            return vec3(0.0);
        }
        h /= std::sqrt(len2);

        real const cos_theta_o = wo.z;
        real const cos_theta_i = wi.z;
        real const cos_theta_d = std::max(0.0, glm::dot(wo, h));

        auto const& lut = ggx_lut();
        real const e_wo = lut.directional(cos_theta_o, effective_alpha(wo));
        real const e_wi = lut.directional(cos_theta_i, effective_alpha(wi));

        vec3 const coat_atten = is_interior ? vec3(1.0) : coat_transmission(cos_theta_o, cos_theta_i);
        vec3 result(0.0);

        real sheen_trans = 1.0;
        real const dielec_weight = is_interior ? 0.0 : (1.0 - m_metallic) * (1.0 - m_spec_trans);

        if (dielec_weight > 0.0) {
            if (m_sheen > 0.0) {
                real const ds = d_charlie(h.z, m_alpha_sheen);
                real const vs = v_ashikhmin(cos_theta_o, cos_theta_i);
                vec3 const f_sheen = m_sheen * m_sheen_color * (ds * vs);
                result += coat_atten * dielec_weight * f_sheen;

                real const es_o = sheen_directional_albedo(cos_theta_o, m_alpha_sheen);
                real const es_i = sheen_directional_albedo(cos_theta_i, m_alpha_sheen);
                sheen_trans = std::max(0.0, (1.0 - m_sheen * es_o) * (1.0 - m_sheen * es_i));
            }

            // Dielectric specular layer transmittance into the diffuse/subsurface substrate
            real const fo_spec = (m_specular > 0.0) ? fresnel_dielectric(cos_theta_o, 1.0 / m_opaque_ior) : 0.0;
            real const fi_spec = (m_specular > 0.0) ? fresnel_dielectric(cos_theta_i, 1.0 / m_opaque_ior) : 0.0;
            real const spec_substrate_trans = (1.0 - fo_spec * e_wo) * (1.0 - fi_spec * e_wi);

            real const fo = schlick_weight(cos_theta_o);
            real const fi = schlick_weight(cos_theta_i);

            real const fd90 = 0.5 + 2.0 * m_roughness * sqr(cos_theta_d);
            real const fd = (1.0 + (fd90 - 1.0) * fo) * (1.0 + (fd90 - 1.0) * fi);

            real const fss90 = m_roughness * sqr(cos_theta_d);
            real const fss = (1.0 + (fss90 - 1.0) * fo) * (1.0 + (fss90 - 1.0) * fi);
            real const ss = 1.25 * (fss * (1.0 / (cos_theta_o + cos_theta_i) - 0.5) + 0.5);

            vec3 const f_diff = m_base_color * std::numbers::inv_pi_v<real> * glm::mix(fd, ss, m_subsurface);
            result += coat_atten * (dielec_weight * sheen_trans * spec_substrate_trans) * f_diff;
        }

        if (!m_is_delta_mf && (m_specular > 0.0 || m_metallic > 0.0 || m_spec_trans > 0.0)) {
            real const d = d_ggx_aniso(h, m_alpha_x, m_alpha_y);
            real const g = g2_smith_ggx_aniso(wo, wi, m_alpha_x, m_alpha_y);
            vec3 const f = specular_fresnel(cos_theta_d, eta, is_interior);

            vec3 const f_single = (f * d * g) / (4.0 * cos_theta_o * cos_theta_i);
            vec3 const f_multi  = is_interior ? vec3(0.0) : (m_ms_energy_factor * (1.0 - e_wo) * (1.0 - e_wi));

            result += coat_atten * sheen_trans * (f_single + f_multi);
        }

        if (!is_interior && m_clearcoat > 0.0) {
            real const dc = d_gtr1(h, m_alpha_coat);
            real const fc = fresnel_dielectric(cos_theta_d, 1.0 / m_clearcoat_ior);
            real const gc = g2_smith_gtr1(wo, wi, m_alpha_coat);
            result += vec3((0.25 * m_clearcoat * dc * fc * gc) / (4.0 * cos_theta_o * cos_theta_i));
        }

        return result;
    }

    auto PrincipledBSDF::pdf_local(const vec3& wo, const vec3& wi, real eta, bool is_interior) const noexcept -> real
    {
        if (wo.z <= 0.0 || std::abs(wi.z) <= 1e-8) {
            return 0.0;
        }

        LobeWeights const weights = compute_lobe_weights(wo.z, eta, is_interior);

        if (wi.z < 0.0) {
            if (m_is_delta_mf || weights.microfacet <= 0.0) return 0.0;

            if (m_thin_walled) {
                vec3 const wi_refl(wi.x, wi.y, -wi.z);
                vec3 h = wo + wi_refl;
                real const len2 = glm::dot(h, h);
                if (len2 <= 1e-16) return 0.0;
                h /= std::sqrt(len2);

                real const wo_dot_h = glm::dot(wo, h);
                if (wo_dot_h <= 0.0) return 0.0;

                real const t_prob = transmission_prob(wo_dot_h, eta, is_interior);
                if (t_prob <= 0.0) return 0.0;

                real const d  = d_ggx_aniso(h, m_alpha_x, m_alpha_y);
                real const g1 = g1_smith_ggx_aniso(wo, m_alpha_x, m_alpha_y);
                return weights.microfacet * t_prob * ((g1 * d) / (4.0 * wo.z));
            }

            vec3 h = (eta < 1.0) ? -(eta * wo + wi) : (eta * wo + wi);
            real const len2 = glm::dot(h, h);
            if (len2 <= 1e-16) return 0.0;
            h /= std::sqrt(len2);
            if (h.z <= 0.0) return 0.0;

            real const wo_dot_h = glm::dot(wo, h);
            real const wi_dot_h = glm::dot(wi, h);
            if (wo_dot_h <= 0.0 || wi_dot_h >= 0.0) return 0.0;

            real const t_prob = transmission_prob(wo_dot_h, eta, is_interior);
            if (t_prob <= 0.0) return 0.0;

            real const d = d_ggx_aniso(h, m_alpha_x, m_alpha_y);
            real const g1 = g1_smith_ggx_aniso(wo, m_alpha_x, m_alpha_y);
            real const d_vndf = (g1 * wo_dot_h * d) / wo.z;

            real const denom = sqr(eta * wo_dot_h + wi_dot_h);
            if (denom <= 1e-16) return 0.0;
            real const jacobian = std::abs(wi_dot_h) / denom;

            return weights.microfacet * t_prob * d_vndf * jacobian;
        }

        vec3 h = wo + wi;
        real const len2 = glm::dot(h, h);
        if (len2 <= 1e-16) return 0.0;
        h /= std::sqrt(len2);

        real const wo_dot_h = std::max(0.0, glm::dot(wo, h));
        real total_pdf = 0.0;

        if (weights.diffuse > 0.0) {
            total_pdf += weights.diffuse * warp::cosine_hemisphere_pdf(wi.z);
        }

        if (!m_is_delta_mf && weights.microfacet > 0.0 && wo_dot_h > 0.0) {
            real const t_prob = transmission_prob(wo_dot_h, eta, is_interior);
            real const d = d_ggx_aniso(h, m_alpha_x, m_alpha_y);
            real const g1 = g1_smith_ggx_aniso(wo, m_alpha_x, m_alpha_y);
            real const pdf_spec = (g1 * d) / (4.0 * wo.z);
            total_pdf += weights.microfacet * (1.0 - t_prob) * pdf_spec;
        }

        if (weights.clearcoat > 0.0 && wo_dot_h > 0.0) {
            real const dc = d_gtr1(h, m_alpha_coat);
            real const pdf_coat = (dc * h.z) / (4.0 * wo_dot_h);
            total_pdf += weights.clearcoat * pdf_coat;
        }

        return total_pdf;
    }

    auto PrincipledBSDF::sample(
        const Ray& ray,
        const HitRecord& record,
        const MaterialSample& sample
    ) const noexcept -> std::optional<BSDFSample>
    {
        ShadingFrame const frame(record);
        vec3 const wo = frame.to_local(-glm::normalize(ray.direction()));
        if (wo.z <= 1e-8) {
            return std::nullopt;
        }

        bool const is_interior = (!record.front_face && !m_thin_walled && m_spec_trans > 0.0);
        real const eta = is_interior ? m_ior : (1.0 / m_ior);
        LobeWeights const weights = compute_lobe_weights(wo.z, eta, is_interior);

        vec3 wi(0.0);
        real const u_lobe = sample.lobe;

        if (u_lobe < weights.diffuse) {
            wi = warp::cosine_hemisphere(sample.direction);
        } else if (u_lobe < weights.diffuse + weights.microfacet) {
            real const u_sub = (u_lobe - weights.diffuse) / std::max(1e-12, weights.microfacet);
            vec3 const h = m_is_delta_mf
                ? vec3(0.0, 0.0, 1.0)
                : sample_ggx_vndf(wo, m_alpha_x, m_alpha_y, sample.direction);

            real const wo_dot_h = glm::dot(wo, h);
            if (wo_dot_h <= 0.0) {
                return std::nullopt;
            }

            real const t_prob = transmission_prob(wo_dot_h, eta, is_interior);
            if (u_sub < t_prob) {
                if (m_thin_walled) {
                    vec3 const wi_refl = glm::reflect(-wo, h);
                    if (wi_refl.z <= 1e-8) {
                        return std::nullopt;
                    }
                    wi = vec3(wi_refl.x, wi_refl.y, -wi_refl.z);

                    if (m_is_delta_mf) {
                        real const f_thin = fresnel_thin_dielectric(wo_dot_h, eta);
                        real const trans_scale = (1.0 - m_metallic) * m_spec_trans;
                        vec3 const coat_atten = coat_transmission(wo.z, wi_refl.z);
                        real const sample_pdf = weights.microfacet * t_prob;
                        vec3 const weight = (coat_atten * m_thin_trans_color * (trans_scale * (1.0 - f_thin))) / sample_pdf;
                        return BSDFSample {
                            .direction = frame.to_world(wi),
                            .weight    = weight,
                            .pdf       = sample_pdf,
                            .is_delta  = true
                        };
                    }
                } else {
                    wi = glm::refract(-wo, h, eta);
                    if (glm::dot(wi, wi) <= 1e-12 || wi.z >= -1e-8) {
                        return std::nullopt;
                    }
                    wi = glm::normalize(wi);

                    if (m_is_delta_mf) {
                        real const f_diel = fresnel_dielectric(wo_dot_h, eta);
                        real const trans_scale = (1.0 - m_metallic) * m_spec_trans;
                        vec3 const coat_atten = is_interior ? vec3(1.0) : coat_transmission(wo.z, std::abs(wi.z));
                        real const sample_pdf = weights.microfacet * t_prob;
                        vec3 const weight = coat_atten * ((sqr(eta) * trans_scale * (1.0 - f_diel)) / sample_pdf);
                        return BSDFSample {
                            .direction = frame.to_world(wi),
                            .weight    = weight,
                            .pdf       = sample_pdf,
                            .is_delta  = true
                        };
                    }
                }
            } else {
                wi = glm::reflect(-wo, h);
                if (wi.z <= 1e-8) {
                    return std::nullopt;
                }

                if (m_is_delta_mf) {
                    vec3 const f = specular_fresnel(wo_dot_h, eta, is_interior);
                    vec3 const coat_atten = is_interior ? vec3(1.0) : coat_transmission(wo.z, wi.z);
                    real const sample_pdf = weights.microfacet * (1.0 - t_prob);
                    vec3 const weight = (coat_atten * f) / sample_pdf;
                    return BSDFSample {
                        .direction = frame.to_world(wi),
                        .weight    = weight,
                        .pdf       = sample_pdf,
                        .is_delta  = true
                    };
                }
            }
        } else {
            vec3 const h = sample_gtr1(m_alpha_coat, sample.direction);
            wi = glm::reflect(-wo, h);
            if (wi.z <= 1e-8) {
                return std::nullopt;
            }
        }

        real const p = pdf_local(wo, wi, eta, is_interior);
        if (p <= 1e-12) {
            return std::nullopt;
        }

        vec3 const f = eval_local(wo, wi, eta, is_interior);
        vec3 const weight = f * (std::abs(wi.z) / p);

        return BSDFSample {
            .direction = frame.to_world(wi),
            .weight    = weight,
            .pdf       = p,
            .is_delta  = false
        };
    }

    auto PrincipledBSDF::eval(const vec3& wo_world, const vec3& wi_world, const HitRecord& record) const noexcept -> vec3
    {
        ShadingFrame const frame(record);
        vec3 const wo = frame.to_local(wo_world);
        vec3 const wi = frame.to_local(wi_world);
        bool const is_interior = (!record.front_face && !m_thin_walled && m_spec_trans > 0.0);
        real const eta = is_interior ? m_ior : (1.0 / m_ior);
        return eval_local(wo, wi, eta, is_interior);
    }

    auto PrincipledBSDF::pdf(const vec3& wo_world, const vec3& wi_world, const HitRecord& record) const noexcept -> real
    {
        ShadingFrame const frame(record);
        vec3 const wo = frame.to_local(wo_world);
        vec3 const wi = frame.to_local(wi_world);
        bool const is_interior = (!record.front_face && !m_thin_walled && m_spec_trans > 0.0);
        real const eta = is_interior ? m_ior : (1.0 / m_ior);
        return pdf_local(wo, wi, eta, is_interior);
    }

}