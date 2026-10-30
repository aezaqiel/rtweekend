#include "principled.hpp"

#include <algorithm>
#include <numbers>

namespace rtweekend {

    namespace {

        inline constexpr real s_delta_alpha_threshold = 1e-3;
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

        [[nodiscard]] constexpr auto fresnel_schlick(real f0, real cos_theta) noexcept -> real
        {
            return f0 + (1.0 - f0) * schlick_weight(cos_theta);
        }

        [[nodiscard]] constexpr auto fresnel_schlick(const vec3& f0, real cos_theta) noexcept -> vec3
        {
            return f0 + (vec3(1.0) - f0) * schlick_weight(cos_theta);
        }

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

        [[nodiscard]] inline auto g1_smith_ggx_aniso(const vec3& v, real ax, real ay) noexcept -> real
        {
            if (v.z == 0.0) return 0.0;
            real const tan2_theta = (sqr(v.x * ax) + sqr(v.y * ay)) / sqr(v.z);

            return 2.0 / (1.0 + std::sqrt(1.0 + tan2_theta));
        }

        [[nodiscard]] inline auto g2_smith_ggx_aniso(const vec3& wo, const vec3& wi, real ax, real ay) noexcept -> real
        {
            return g1_smith_ggx_aniso(wo, ax, ay) * g1_smith_ggx_aniso(wi, ax, ay);
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
        , m_ior(std::max(1.001, config.ior))
        , m_spec_trans(std::clamp(config.spec_trans, 0.0, 1.0))
        , m_emission(glm::max(config.emission, vec3(0.0)))
    {
        real const aspect = std::sqrt(1.0 - 0.9 * m_anisotropic);
        real const r2 = m_roughness * m_roughness;

        m_alpha_x = std::max(s_min_alpha, r2 / aspect);
        m_alpha_y = std::max(s_min_alpha, r2 * aspect);
        m_alpha_coat = std::max(0.001, sqr(m_clearcoat_roughness));

        real const lum = rec709_luminance(m_base_color);

        m_tint = (lum > 1e-6) ? (m_base_color / lum) : vec3(1.0);
        m_spec_tint_color = glm::mix(vec3(1.0), m_tint, m_specular_tint);
        m_sheen_color = glm::mix(vec3(1.0), m_tint, m_sheen_tint);
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

    auto PrincipledBSDF::dielectric(real ior, real roughness, const vec3& base_color) -> std::shared_ptr<PrincipledBSDF>
    {
        return std::make_shared<PrincipledBSDF>(Config {
            .base_color = base_color,
            .metallic = 0.0,
            .roughness = roughness,
            .ior = ior,
            .spec_trans = 1.0
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
        real const dielec_weight = (1.0 - m_metallic) * (1.0 - m_spec_trans);
        real const w_diff = dielec_weight * std::max(0.05, rec709_luminance(m_base_color));

        real const f_diel = fresnel_dielectric(cos_theta_o, eta);
        real const r0_artistic = 0.08 * m_specular;
        real const f_opaque = fresnel_schlick(r0_artistic, cos_theta_o);
        real const f_spec_est = glm::mix(glm::mix(f_opaque, f_diel, m_spec_trans), 1.0, m_metallic);
        real const t_est = (1.0 - m_metallic) * m_spec_trans * (1.0 - f_diel);

        real const w_mf = std::max(0.0, f_spec_est + t_est);
        real const w_coat = 0.25 * m_clearcoat * fresnel_schlick(0.04, cos_theta_o);

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
        vec3 const f0_artistic = (0.08 * m_specular) * m_spec_tint_color;
        vec3 const f_opaque = fresnel_schlick(f0_artistic, cos_theta_d);
        vec3 const f_dielectric_blend = glm::mix(f_opaque, f_diel * m_spec_tint_color, m_spec_trans);
        vec3 const f_metal = fresnel_schlick(m_base_color, cos_theta_d);

        return glm::mix(f_dielectric_blend, f_metal, m_metallic);
    }

    auto PrincipledBSDF::eval_local(const vec3& wo, const vec3& wi, real eta) const noexcept -> vec3
    {
        if (wo.z <= 0.0 || std::abs(wi.z) <= 1e-8) {
            return vec3(0.0);
        }

        if (wi.z < 0.0) {
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
            real const g = g2_smith_ggx_aniso(wo, wi, m_alpha_x, m_alpha_y);
            real const denom = sqr(eta * wo_dot_h + wi_dot_h) * wo.z * std::abs(wi.z);

            if (denom <= 1e-16) {
                return vec3(0.0);
            }

            vec3 const tint = glm::sqrt(m_base_color);
            real const factor = (d * g * (1.0 - f_diel) * wo_dot_h * std::abs(wi_dot_h)) / denom;

            return trans_scale * tint * factor;
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

            result += dielec_weight * (f_diff + f_sheen);
        }

        if (m_specular > 0.0 || m_metallic > 0.0 || m_spec_trans > 0.0) {
            real const d = d_ggx_aniso(h, m_alpha_x, m_alpha_y);
            real const g = g2_smith_ggx_aniso(wo, wi, m_alpha_x, m_alpha_y);
            vec3 const f = specular_fresnel(cos_theta_d, eta);
            result += (f * d * g) / (4.0 * cos_theta_o * cos_theta_i);
        }

        if (m_clearcoat > 0.0) {
            real const dc = d_gtr1(h, m_alpha_coat);
            real const fc = fresnel_schlick(0.04, cos_theta_d);
            real const gc = g2_smith_ggx_aniso(wo, wi, 0.25, 0.25);
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
            if (weights.microfacet <= 0.0) return 0.0;

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

        if (weights.microfacet > 0.0 && wo_dot_h > 0.0) {
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
        ShadingFrame const frame(record.normal);
        vec3 const wo = frame.to_local(-glm::normalize(ray.direction()));

        if (wo.z <= 1e-8) {
            return std::nullopt;
        }

        real const eta = record.front_face ? (1.0 / m_ior) : m_ior;
        LobeWeights const weights = compute_lobe_weights(wo.z, eta);
        bool const is_smooth_mf = std::max(m_alpha_x, m_alpha_y) <= s_delta_alpha_threshold;

        vec3 wi(0.0);
        real const u_lobe = sample.lobe;

        if (u_lobe < weights.diffuse) {
            wi = warp::cosine_hemisphere(sample.direction);
        } else if (u_lobe < weights.diffuse + weights.microfacet) {
            real const u_sub = (u_lobe - weights.diffuse) / std::max(1e-12, weights.microfacet);
            vec3 const h = is_smooth_mf
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

                if (is_smooth_mf) {
                    real const f_diel = fresnel_dielectric(wo_dot_h, eta);
                    real const g1_i = g1_smith_ggx_aniso(wi, m_alpha_x, m_alpha_y);
                    vec3 const tint = glm::sqrt(m_base_color);
                    vec3 const weight = ((1.0 - m_metallic) * m_spec_trans * tint * (1.0 - f_diel) * g1_i)
                                      / (weights.microfacet * t_prob);

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

                if (is_smooth_mf) {
                    vec3 const f = specular_fresnel(wo_dot_h, eta);
                    real const g1_i = g1_smith_ggx_aniso(wi, m_alpha_x, m_alpha_y);
                    vec3 const weight = (f * g1_i) / (weights.microfacet * (1.0 - t_prob));

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
        ShadingFrame const frame(record.normal);

        vec3 const wo = frame.to_local(wo_world);
        vec3 const wi = frame.to_local(wi_world);
        real const eta = record.front_face ? (1.0 / m_ior) : m_ior;

        return eval_local(wo, wi, eta);
    }

    auto PrincipledBSDF::pdf(const vec3& wo_world, const vec3& wi_world, const HitRecord& record) const -> real
    {
        ShadingFrame const frame(record.normal);

        vec3 const wo = frame.to_local(wo_world);
        vec3 const wi = frame.to_local(wi_world);
        real const eta = record.front_face ? (1.0 / m_ior) : m_ior;

        return pdf_local(wo, wi, eta);
    }

}
