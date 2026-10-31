#include "principled.hpp"

#include <algorithm>
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

        // Exact Maxwell unpolarized Fresnel reflectance for a dielectric interface
        // where eta = eta_i / eta_t.
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

        [[nodiscard]] constexpr auto rec709_luminance(const vec3& c) noexcept -> real
        {
            return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z;
        }

        [[nodiscard]] inline auto d_ggx_aniso(const vec3& h, real ax, real ay) noexcept -> real
        {
            if (h.z <= 0.0) return 0.0;
            real const term = sqr(h.x / ax) + sqr(h.y / ay) + sqr(h.z);
            return 1.0 / (std::numbers::pi_v<real> * ax * ay * sqr(term));
        }

        // Exact Smith Lambda for anisotropic GGX
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

        // Heitz (2014) Exact Height-Correlated Anisotropic Smith G2
        [[nodiscard]] inline auto g2_smith_ggx_aniso(const vec3& wo, const vec3& wi, real ax, real ay) noexcept -> real
        {
            if (wo.z == 0.0 || wi.z == 0.0) return 0.0;
            return 1.0 / (1.0 + lambda_smith_ggx_aniso(wo, ax, ay) + lambda_smith_ggx_aniso(wi, ax, ay));
        }

        // Exact directional albedo E(wo) of the single-scattering GGX microfacet BRDF (with F=1)
        // integrated via 8-point Gauss-Legendre quadrature over the 2D VNDF hemisphere.
        [[nodiscard]] inline auto ggx_directional_albedo(const vec3& wo, real ax, real ay) noexcept -> real
        {
            if (wo.z <= 1e-6) return 0.0;

            static constexpr real s_nodes[4] = {
                0.1834346424956498, 0.5255324099163290, 0.7966664774136267, 0.9602898564975363
            };

            static constexpr real s_weights[4] = {
                0.3626837833783620, 0.3137066458778873, 0.2223810344533745, 0.1012285362903763
            };

            vec3 const vh = glm::normalize(vec3(ax * wo.x, ay * wo.y, wo.z));
            real const lensq = vh.x * vh.x + vh.y * vh.y;
            vec3 const t1 = lensq > 1e-12
                ? vec3(-vh.y, vh.x, 0.0) / std::sqrt(lensq)
                : vec3(1.0, 0.0, 0.0);
            vec3 const t2 = glm::cross(vh, t1);
            real const s = 0.5 * (1.0 + vh.z);
            real const lambda_o = lambda_smith_ggx_aniso(wo, ax, ay);

            auto eval_vndf_point = [&](real r, real phi) noexcept -> real {
                real const px = r * std::cos(phi);
                real py = r * std::sin(phi);
                py = (1.0 - s) * std::sqrt(std::max(0.0, 1.0 - px * px)) + s * py;
                real const pz = std::sqrt(std::max(0.0, 1.0 - px * px - py * py));
                vec3 const nh = px * t1 + py * t2 + pz * vh;
                vec3 const h = glm::normalize(vec3(ax * nh.x, ay * nh.y, std::max(1e-6, nh.z)));
                vec3 const wi = glm::reflect(-wo, h);
                if (wi.z <= 0.0) return 0.0;
                real const lambda_i = lambda_smith_ggx_aniso(wi, ax, ay);
                // Ratio G2(wo, wi) / G1(wo) = (1 + lambda_o) / (1 + lambda_o + lambda_i)
                return (1.0 + lambda_o) / (1.0 + lambda_o + lambda_i);
            };

            real integral = 0.0;
            for (int i = 0; i < 4; ++i) {
                for (int sign_r : { -1, 1 }) {
                    real const u_r = 0.5 + 0.5 * sign_r * s_nodes[i];
                    real const r = std::sqrt(u_r);
                    real const wr = 0.5 * s_weights[i];
                    for (int j = 0; j < 4; ++j) {
                        for (int sign_p : { -1, 1 }) {
                            real const u_p = 0.5 + 0.5 * sign_p * s_nodes[j];
                            real const phi = 2.0 * std::numbers::pi_v<real> * u_p;
                            real const wp = 0.5 * s_weights[j];
                            integral += wr * wp * eval_vndf_point(r, phi);
                        }
                    }
                }
            }
            return std::clamp(integral, 1e-4, 1.0);
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

        [[nodiscard]] inline auto d_gtr1(const vec3& h, real alpha) noexcept -> real
        {
            if (h.z <= 0.0) return 0.0;
            if (alpha >= 1.0) return std::numbers::inv_pi_v<real>;
            real const a2 = alpha * alpha;
            real const denom = 1.0 + (a2 - 1.0) * h.z * h.z;
            return (a2 - 1.0) / (std::numbers::pi_v<real> * std::log(a2) * denom);
        }

        // Exact analytical Smith Lambda for the GTR1 (Berry) distribution
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

        // Exact Height-Correlated Smith G2 for the GTR1 (Berry) clearcoat distribution
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

    PrincipledBSDF::PrincipledBSDF(const Config& config)
        : m_base_color(glm::clamp(config.base_color, vec3(0.0), vec3(1.0)))
        , m_metallic(std::clamp(config.metallic, 0.0, 1.0))
        , m_roughness(std::clamp(config.roughness, 0.0, 1.0))
        , m_subsurface(std::clamp(config.subsurface, 0.0, 1.0))
        , m_specular(std::max(0.0, config.specular))
        , m_specular_tint(std::clamp(config.specular_tint, 0.0, 1.0))
        , m_anisotropic(std::clamp(config.anisotropic, 0.0, 1.0))
        , m_sheen(std::max(0.0, config.sheen))
        , m_sheen_tint(std::clamp(config.sheen_tint, 0.0, 1.0))
        , m_clearcoat(std::max(0.0, config.clearcoat))
        , m_clearcoat_roughness(std::clamp(config.clearcoat_roughness, 0.0, 1.0))
        , m_clearcoat_ior(std::max(1.001, config.clearcoat_ior))
        , m_ior(std::max(1.001, config.ior))
        , m_spec_trans(std::clamp(config.spec_trans, 0.0, 1.0))
        , m_emission(glm::max(config.emission, vec3(0.0)))
    {
        real const aspect = std::sqrt(1.0 - 0.9 * m_anisotropic);
        real const r2 = m_roughness * m_roughness;
        m_is_delta_mf = (r2 <= 1e-7);
        m_alpha_x = std::max(s_min_alpha, r2 / aspect);
        m_alpha_y = std::max(s_min_alpha, r2 * aspect);
        m_alpha_coat = std::max(0.001, sqr(m_clearcoat_roughness));

        real const lum = rec709_luminance(m_base_color);
        m_tint = (lum > 1e-6) ? (m_base_color / lum) : vec3(1.0);
        m_spec_tint_color = glm::mix(vec3(1.0), m_tint, m_specular_tint);
        m_sheen_color = glm::mix(vec3(1.0), m_tint, m_sheen_tint);

        // Exact dielectric IOR corresponding to Disney's artistic F0 = 0.08 * specular
        real const f0_spec = std::clamp(0.08 * m_specular, 0.0, 0.99);
        m_opaque_ior = (2.0 / (1.0 - std::sqrt(f0_spec))) - 1.0;

        // Exact complex conductor IOR (eta, k) via Gulbrandsen's analytical inversion
        gulbrandsen_ior_from_artistic(m_base_color, m_spec_tint_color, m_conductor_eta, m_conductor_k);

        // Exact Beer-Lambert volumetric absorption coefficient sigma_a = -ln(T) / d
        if (m_spec_trans > 0.0 && m_metallic < 1.0) {
            real const atten_dist = std::max(1e-6, config.attenuation_distance);
            vec3 const clamped_color = glm::clamp(m_base_color, vec3(1e-6), vec3(1.0));
            m_sigma_a = -glm::log(clamped_color) / atten_dist;
        }
    }

    auto PrincipledBSDF::diffuse(const vec3& base_color, real roughness, real subsurface) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(Config {
            .base_color = base_color,
            .metallic = 0.0,
            .roughness = roughness,
            .subsurface = subsurface,
            .specular = 0.0
        });
    }

    auto PrincipledBSDF::metal(const vec3& base_color, real roughness, real anisotropic) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(Config {
            .base_color = base_color,
            .metallic = 1.0,
            .roughness = roughness,
            .anisotropic = anisotropic
        });
    }

    auto PrincipledBSDF::dielectric(real ior, real roughness, const vec3& base_color, real attenuation_distance) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(Config {
            .base_color = base_color,
            .metallic = 0.0,
            .roughness = roughness,
            .ior = ior,
            .spec_trans = 1.0,
            .attenuation_distance = attenuation_distance
        });
    }

    auto PrincipledBSDF::plastic(const vec3& base_color, real roughness, real clearcoat) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(Config {
            .base_color = base_color,
            .metallic = 0.0,
            .roughness = roughness,
            .specular = 0.5,
            .clearcoat = clearcoat,
            .clearcoat_roughness = 0.03
        });
    }

    auto PrincipledBSDF::emissive(const vec3& radiance) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(Config {
            .base_color = vec3(0.0),
            .specular = 0.0,
            .emission = radiance
        });
    }

    auto PrincipledBSDF::compute_lobe_weights(real cos_theta_o, real eta) const noexcept -> LobeWeights
    {
        real const f_coat = fresnel_dielectric(cos_theta_o, 1.0 / m_clearcoat_ior);
        real const coat_trans = std::max(0.0, 1.0 - 0.25 * m_clearcoat * f_coat);

        real const dielec_weight = (1.0 - m_metallic) * (1.0 - m_spec_trans);
        real const w_diff = coat_trans * dielec_weight * std::max(0.05, rec709_luminance(m_base_color));

        real const f_diel = fresnel_dielectric(cos_theta_o, eta);
        real const f_opaque = (m_specular > 0.0) ? fresnel_dielectric(cos_theta_o, 1.0 / m_opaque_ior) : 0.0;
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

    auto PrincipledBSDF::transmission_prob(real cos_theta_d, real eta) const noexcept -> real
    {
        real const trans_scale = (1.0 - m_metallic) * m_spec_trans;
        if (trans_scale <= 0.0) {
            return 0.0;
        }
        real const f_diel = fresnel_dielectric(cos_theta_d, eta);
        real const t_weight = trans_scale * (1.0 - f_diel);
        real const r_weight = glm::mix(f_diel, 1.0, m_metallic);
        real const denom = t_weight + r_weight;
        return (denom > 1e-12) ? (t_weight / denom) : 0.0;
    }

    auto PrincipledBSDF::specular_fresnel(real cos_theta_d, real eta) const noexcept -> vec3
    {
        real const f_diel = fresnel_dielectric(cos_theta_d, eta);
        real const f_opaque_scalar = (m_specular > 0.0) ? fresnel_dielectric(cos_theta_d, 1.0 / m_opaque_ior) : 0.0;
        vec3 const f_opaque = f_opaque_scalar * glm::mix(m_spec_tint_color, vec3(1.0), f_opaque_scalar);
        vec3 const f_dielectric_blend = glm::mix(f_opaque, f_diel * m_spec_tint_color, m_spec_trans);
        vec3 const f_metal = fresnel_conductor(cos_theta_d, m_conductor_eta, m_conductor_k);
        return glm::mix(f_dielectric_blend, f_metal, m_metallic);
    }

    auto PrincipledBSDF::eval_local(const vec3& wo, const vec3& wi, real eta) const noexcept -> vec3
    {
        if (wo.z <= 0.0 || std::abs(wi.z) <= 1e-8) {
            return vec3(0.0);
        }

        // Continuous evaluation of transmission (only non-zero when microfacet is non-delta)
        if (wi.z < 0.0) {
            if (m_is_delta_mf) {
                return vec3(0.0);
            }
            real const trans_scale = (1.0 - m_metallic) * m_spec_trans;
            if (trans_scale <= 0.0) {
                return vec3(0.0);
            }

            vec3 h = eta * wo + wi;
            real const len2 = glm::dot(h, h);
            if (len2 <= 1e-16) {
                return vec3(0.0);
            }
            h /= std::sqrt(len2);
            if (h.z < 0.0) {
                h = -h;
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
            return vec3(trans_scale * factor);
        }

        vec3 h = wo + wi;
        real const len2 = glm::dot(h, h);
        if (len2 <= 1e-16) {
            return vec3(0.0);
        }
        h /= std::sqrt(len2);

        real const cos_theta_o = wo.z;
        real const cos_theta_i = wi.z;
        real const cos_theta_d = std::max(0.0, glm::dot(wo, h));

        // Clearcoat layer transmittance attenuates all underlying base lobes to conserve energy
        real const fc_o = fresnel_dielectric(cos_theta_o, 1.0 / m_clearcoat_ior);
        real const fc_i = fresnel_dielectric(cos_theta_i, 1.0 / m_clearcoat_ior);
        real const coat_attenuation = (1.0 - 0.25 * m_clearcoat * fc_o) * (1.0 - 0.25 * m_clearcoat * fc_i);

        vec3 result(0.0);

        real const dielec_weight = (1.0 - m_metallic) * (1.0 - m_spec_trans);
        if (dielec_weight > 0.0) {
            real const fo = schlick_weight(cos_theta_o);
            real const fi = schlick_weight(cos_theta_i);

            real const fd90 = 0.5 + 2.0 * m_roughness * sqr(cos_theta_d);
            real const fd = (1.0 + (fd90 - 1.0) * fo) * (1.0 + (fd90 - 1.0) * fi);

            real const fss90 = m_roughness * sqr(cos_theta_d);
            real const fss = (1.0 + (fss90 - 1.0) * fo) * (1.0 + (fss90 - 1.0) * fi);
            real const ss = 1.25 * (fss * (1.0 / (cos_theta_o + cos_theta_i) - 0.5) + 0.5);

            vec3 const f_diff = m_base_color * std::numbers::inv_pi_v<real> * glm::mix(fd, ss, m_subsurface);
            vec3 const f_sheen = m_sheen * m_sheen_color * schlick_weight(cos_theta_d);

            result += coat_attenuation * dielec_weight * (f_diff + f_sheen);
        }

        if (!m_is_delta_mf && (m_specular > 0.0 || m_metallic > 0.0 || m_spec_trans > 0.0)) {
            real const d = d_ggx_aniso(h, m_alpha_x, m_alpha_y);
            real const g = g2_smith_ggx_aniso(wo, wi, m_alpha_x, m_alpha_y);
            vec3 const f = specular_fresnel(cos_theta_d, eta);

            // Turquin (2019) exact directional albedo multiscatter energy compensation
            real const e_wo = ggx_directional_albedo(wo, m_alpha_x, m_alpha_y);
            vec3 const f_zero = specular_fresnel(1.0, eta);
            vec3 const ms_scale = vec3(1.0) + f_zero * ((1.0 - e_wo) / e_wo);

            result += coat_attenuation * ms_scale * (f * d * g) / (4.0 * cos_theta_o * cos_theta_i);
        }

        if (m_clearcoat > 0.0) {
            real const dc = d_gtr1(h, m_alpha_coat);
            real const fc = fresnel_dielectric(cos_theta_d, 1.0 / m_clearcoat_ior);
            real const gc = g2_smith_gtr1(wo, wi, m_alpha_coat);
            result += vec3((0.25 * m_clearcoat * dc * fc * gc) / (4.0 * cos_theta_o * cos_theta_i));
        }

        return result;
    }

    auto PrincipledBSDF::pdf_local(const vec3& wo, const vec3& wi, real eta) const noexcept -> real
    {
        if (wo.z <= 0.0 || std::abs(wi.z) <= 1e-8) {
            return 0.0;
        }

        LobeWeights const weights = compute_lobe_weights(wo.z, eta);

        if (wi.z < 0.0) {
            if (m_is_delta_mf || weights.microfacet <= 0.0) return 0.0;

            vec3 h = eta * wo + wi;
            real const len2 = glm::dot(h, h);
            if (len2 <= 1e-16) return 0.0;
            h /= std::sqrt(len2);
            if (h.z < 0.0) h = -h;

            real const wo_dot_h = glm::dot(wo, h);
            real const wi_dot_h = glm::dot(wi, h);
            if (wo_dot_h <= 0.0 || wi_dot_h >= 0.0) return 0.0;

            real const t_prob = transmission_prob(wo_dot_h, eta);
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
            real const t_prob = transmission_prob(wo_dot_h, eta);
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

    auto PrincipledBSDF::sample(const Ray& ray, const HitRecord& record, const MaterialSample& sample) const -> std::optional<BSDFSample>
    {
        ShadingFrame const frame(record);
        vec3 const wo = frame.to_local(-glm::normalize(ray.direction()));
        if (wo.z <= 1e-8) {
            return std::nullopt;
        }

        real const eta = record.front_face ? (1.0 / m_ior) : m_ior;
        LobeWeights const weights = compute_lobe_weights(wo.z, eta);

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

            real const t_prob = transmission_prob(wo_dot_h, eta);
            if (u_sub < t_prob) {
                wi = glm::refract(-wo, h, eta);
                if (glm::dot(wi, wi) <= 1e-12 || wi.z >= -1e-8) {
                    return std::nullopt;
                }
                wi = glm::normalize(wi);

                if (m_is_delta_mf) {
                    real const f_diel = fresnel_dielectric(wo_dot_h, eta);
                    real const trans_scale = (1.0 - m_metallic) * m_spec_trans;
                    vec3 const weight = vec3((sqr(eta) * trans_scale * (1.0 - f_diel)) / (weights.microfacet * t_prob));
                    return BSDFSample {
                        .direction = frame.to_world(wi),
                        .weight    = weight,
                        .pdf       = weights.microfacet * t_prob,
                        .is_delta  = true
                    };
                }
            } else {
                wi = glm::reflect(-wo, h);
                if (wi.z <= 1e-8) {
                    return std::nullopt;
                }

                if (m_is_delta_mf) {
                    vec3 const f = specular_fresnel(wo_dot_h, eta);
                    real const fc_o = fresnel_dielectric(wo.z, 1.0 / m_clearcoat_ior);
                    real const coat_attenuation = sqr(1.0 - 0.25 * m_clearcoat * fc_o);
                    vec3 const weight = (coat_attenuation * f) / (weights.microfacet * (1.0 - t_prob));
                    return BSDFSample {
                        .direction = frame.to_world(wi),
                        .weight    = weight,
                        .pdf       = weights.microfacet * (1.0 - t_prob),
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

        real const p = pdf_local(wo, wi, eta);
        if (p <= 1e-12) {
            return std::nullopt;
        }

        vec3 const f = eval_local(wo, wi, eta);
        vec3 const weight = f * (std::abs(wi.z) / p);

        return BSDFSample {
            .direction = frame.to_world(wi),
            .weight    = weight,
            .pdf       = p,
            .is_delta  = false
        };
    }

    auto PrincipledBSDF::eval(const vec3& wo_world, const vec3& wi_world, const HitRecord& record) const -> vec3
    {
        ShadingFrame const frame(record);
        vec3 const wo = frame.to_local(wo_world);
        vec3 const wi = frame.to_local(wi_world);
        real const eta = record.front_face ? (1.0 / m_ior) : m_ior;
        return eval_local(wo, wi, eta);
    }

    auto PrincipledBSDF::pdf(const vec3& wo_world, const vec3& wi_world, const HitRecord& record) const -> real
    {
        ShadingFrame const frame(record);
        vec3 const wo = frame.to_local(wo_world);
        vec3 const wi = frame.to_local(wi_world);
        real const eta = record.front_face ? (1.0 / m_ior) : m_ior;
        return pdf_local(wo, wi, eta);
    }

}