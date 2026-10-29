#include "film.hpp"

#include <numbers>
#include <algorithm>

#include <stb_image_write.h>

namespace rtweekend {

    namespace {

        inline constexpr real s_filter_radius = 1.5;
        inline constexpr real s_inv_filter_radius = 1.0 / s_filter_radius;

        [[nodiscard]] inline auto blackman_harris_1d(real d) noexcept -> real
        {
            real const ad = std::abs(d);
            if (ad >= s_filter_radius) return 0.0;

            constexpr real a0 = 0.35875;
            constexpr real a1 = 0.48829;
            constexpr real a2 = 0.14128;
            constexpr real a3 = 0.01168;

            real const phase = std::numbers::pi_v<real> * (d * s_inv_filter_radius + 1.0);

            return std::max(0.0, a0 - a1 * std::cos(phase) + a2 * std::cos(2.0 * phase) - a3 * std::cos(3.0 * phase));
        }

        [[nodiscard]] inline auto evaluate_filter(real dx, real dy) noexcept -> real
        {
            return blackman_harris_1d(dx) * blackman_harris_1d(dy);
        }

        constexpr auto neumaier_add(real& sum, real& comp, real value) noexcept -> void
        {
            real const t = sum + value;
            if (std::abs(sum) >= std::abs(value)) {
                comp += (sum - t) + value;
            } else {
                comp += (value - t) + sum;
            }
            sum = t;
        }

        constexpr auto neumaier_add(vec3& sum, vec3& comp, const vec3& value) noexcept -> void
        {
            neumaier_add(sum.x, comp.x, value.x);
            neumaier_add(sum.y, comp.y, value.y);
            neumaier_add(sum.z, comp.z, value.z);
        }

        [[nodiscard]] constexpr auto mat3_rows(
            real m00, real m01, real m02,
            real m10, real m11, real m12,
            real m20, real m21, real m22
        ) noexcept -> mat3
        {
            return mat3(
                m00, m10, m20,
                m01, m11, m21,
                m02, m12, m22
            );
        }

        [[nodiscard]] constexpr auto xy_to_XYZ(real x, real y) noexcept -> vec3
        {
            return vec3(x / y, 1.0, (1.0 - x - y) / y);
        }

        [[nodiscard]] inline auto rgb_to_xyz_matrix(vec2 r_xy, vec2 g_xy, vec2 b_xy, vec2 w_xy) noexcept -> mat3
        {
            vec3 const R = xy_to_XYZ(r_xy.x, r_xy.y);
            vec3 const G = xy_to_XYZ(g_xy.x, g_xy.y);
            vec3 const B = xy_to_XYZ(b_xy.x, b_xy.y);
            vec3 const W = xy_to_XYZ(w_xy.x, w_xy.y);

            mat3 const P(R, G, B);
            vec3 const S = glm::inverse(P) * W;

            return mat3(R * S.x, G * S.y, B * S.z);
        }

        [[nodiscard]] inline auto saturation_matrix(real sat, const vec3& luma) noexcept -> mat3
        {
            vec3 const w = (1.0 - sat) * luma;

            return mat3_rows(
                w.x + sat, w.y,       w.z,
                w.x,       w.y + sat, w.z,
                w.x,       w.y,       w.z + sat
            );
        }

        struct ColorPipelineMatrices
        {
            mat3 rec709_to_ap0;
            mat3 ap0_to_ap1;
            mat3 ap1_to_xyz;
            mat3 xyz_to_ap1;
            mat3 ap1_to_xyz_d65;
            mat3 xyz_to_rec709;
            mat3 rrt_sat;
            mat3 odt_sat;
            vec3 rec709_luma;

            ColorPipelineMatrices() noexcept
            {
                mat3 const rec709_to_xyz = rgb_to_xyz_matrix(
                    { 0.64, 0.33 }, { 0.30, 0.60 }, { 0.15, 0.06 }, { 0.3127, 0.3290 }
                );
                xyz_to_rec709 = glm::inverse(rec709_to_xyz);

                mat3 const ap0_to_xyz = rgb_to_xyz_matrix(
                    { 0.7347, 0.2653 }, { 0.0000, 1.0000 }, { 0.0001, -0.0770 }, { 0.32168, 0.33767 }
                );
                mat3 const xyz_to_ap0 = glm::inverse(ap0_to_xyz);
                
                ap1_to_xyz = rgb_to_xyz_matrix(
                    { 0.713, 0.293 }, { 0.165, 0.830 }, { 0.128, 0.044 }, { 0.32168, 0.33767 }
                );
                xyz_to_ap1 = glm::inverse(ap1_to_xyz);

                mat3 const bradford = mat3_rows(
                     0.8951,  0.2664, -0.1614,
                    -0.7502,  1.7135,  0.0367,
                     0.0389, -0.0685,  1.0296
                );
                mat3 const inv_bradford = glm::inverse(bradford);

                vec3 const cone_d65 = bradford * xy_to_XYZ(0.3127, 0.3290);
                vec3 const cone_d60 = bradford * xy_to_XYZ(0.32168, 0.33767);
                vec3 const ratio = cone_d60 / cone_d65;

                mat3 const d65_to_d60 = inv_bradford * mat3(
                    ratio.x, 0.0, 0.0,
                    0.0, ratio.y, 0.0,
                    0.0, 0.0, ratio.z
                ) * bradford;
                mat3 const d60_to_d65 = glm::inverse(d65_to_d60);

                rec709_to_ap0  = xyz_to_ap0 * d65_to_d60 * rec709_to_xyz;
                ap0_to_ap1     = xyz_to_ap1 * ap0_to_xyz;
                ap1_to_xyz_d65 = d60_to_d65 * ap1_to_xyz;

                rec709_luma = vec3(rec709_to_xyz[0][1], rec709_to_xyz[1][1], rec709_to_xyz[2][1]);
                vec3 const ap1_luma(ap1_to_xyz[0][1], ap1_to_xyz[1][1], ap1_to_xyz[2][1]);

                rrt_sat = saturation_matrix(0.96, ap1_luma);
                odt_sat = saturation_matrix(0.93, ap1_luma);
            }
        };

        [[nodiscard]] auto color_matrices() noexcept -> const ColorPipelineMatrices&
        {
            static const ColorPipelineMatrices s_matrices;
            return s_matrices;
        }

        [[nodiscard]] inline auto luminance(const vec3& rgb) noexcept -> real
        {
            return glm::dot(rgb, color_matrices().rec709_luma);
        }

        [[nodiscard]] constexpr auto aces_rgb_to_yc(const vec3& rgb) noexcept -> real
        {
            real const r = rgb.x;
            real const g = rgb.y;
            real const b = rgb.z;
            real const radicand = b * (b - g) + g * (g - r) + r * (r - b);
            real const chroma = radicand > 0.0 ? std::sqrt(radicand) : 0.0;

            return (b + g + r + 1.75 * chroma) / 3.0;
        }

        [[nodiscard]] constexpr auto aces_rgb_to_saturation(const vec3& rgb) noexcept -> real
        {
            constexpr real tiny = 1e-10;
            real const hi = std::max({ rgb.x, rgb.y, rgb.z, tiny });
            real const lo = std::max({ std::min({ rgb.x, rgb.y, rgb.z }), tiny });
            return (hi - lo) / hi;
        }

        [[nodiscard]] constexpr auto aces_sigmoid_shaper(real x) noexcept -> real
        {
            real const t = std::max(0.0, 1.0 - std::abs(0.5 * x));
            real const s = x < 0.0 ? -1.0 : (x > 0.0 ? 1.0 : 0.0);
            return 0.5 * (1.0 + s * (1.0 - t * t));
        }

        [[nodiscard]] constexpr auto aces_glow_fwd(real yc_in, real glow_gain_in, real glow_mid) noexcept -> real
        {
            if (yc_in <= (2.0 / 3.0) * glow_mid) return glow_gain_in;
            if (yc_in >= 2.0 * glow_mid) return 0.0;
            return glow_gain_in * (glow_mid / yc_in - 0.5);
        }

        [[nodiscard]] constexpr auto aces_cubic_basis_shaper(real x, real width) noexcept -> real
        {
            real const s = (2.0 * std::abs(x)) / width;
            if (s >= 1.0) return 0.0;
            if (s < 0.5) {
                real const t = 2.0 * s;
                return 0.25 * (3.0 * t * t * t - 6.0 * t * t + 4.0);
            }
            real const t = 2.0 * s - 1.0;
            real const omt = 1.0 - t;
            return 0.25 * omt * omt * omt;
        }

        template <usize N>
        [[nodiscard]] auto aces_segmented_spline_eval(
            real x,
            vec2 min_pt,
            vec2 mid_pt,
            vec2 max_pt,
            real slope_low,
            real slope_high,
            const real (&coefs_low)[N],
            const real (&coefs_high)[N]
        ) noexcept -> real
        {
            if (x <= 0.0) return 0.0;

            constexpr usize n_knots = N - 2;
            real const logx = std::log10(x);
            real const logx_min = std::log10(min_pt.x);
            real const logx_mid = std::log10(mid_pt.x);
            real const logx_max = std::log10(max_pt.x);
            real const logy_min = std::log10(min_pt.y);
            real const logy_max = std::log10(max_pt.y);

            real logy = 0.0;
            if (logx <= logx_min) {
                logy = logx * slope_low + (logy_min - slope_low * logx_min);
            } else if (logx < logx_mid) {
                real const knot_coord = static_cast<real>(n_knots - 1) * (logx - logx_min) / (logx_mid - logx_min);
                usize const j = std::min(static_cast<usize>(knot_coord), n_knots - 2);
                real const t = knot_coord - static_cast<real>(j);
                real const c0 = coefs_low[j];
                real const c1 = coefs_low[j + 1];
                real const c2 = coefs_low[j + 2];
                logy = 0.5 * (c0 + c1) + t * (c1 - c0) + 0.5 * t * t * (c0 - 2.0 * c1 + c2);
            } else if (logx < logx_max) {
                real const knot_coord = static_cast<real>(n_knots - 1) * (logx - logx_mid) / (logx_max - logx_mid);
                usize const j = std::min(static_cast<usize>(knot_coord), n_knots - 2);
                real const t = knot_coord - static_cast<real>(j);
                real const c0 = coefs_high[j];
                real const c1 = coefs_high[j + 1];
                real const c2 = coefs_high[j + 2];
                logy = 0.5 * (c0 + c1) + t * (c1 - c0) + 0.5 * t * t * (c0 - 2.0 * c1 + c2);
            } else {
                logy = logx * slope_high + (logy_max - slope_high * logx_max);
            }

            return std::pow(10.0, logy);
        }

        [[nodiscard]] auto aces_spline_c5_fwd(real x) noexcept -> real
        {
            static constexpr real s_coefs_low[6] = {
                -4.0000000000, -4.0000000000, -3.1573765773,
                -0.4852499958,  1.8477324706,  1.8477324706
            };
            static constexpr real s_coefs_high[6] = {
                -0.7185482425,  2.0810307172,  3.6681241237,
                 4.0000000000,  4.0000000000,  4.0000000000
            };

            return aces_segmented_spline_eval(
                x,
                { 0.18 * std::exp2(-15.0), 0.0001 },
                { 0.18, 4.8 },
                { 0.18 * std::exp2(18.0), 10000.0 },
                0.0, 0.0,
                s_coefs_low, s_coefs_high
            );
        }

        [[nodiscard]] auto aces_spline_c9_fwd(real x) noexcept -> real
        {
            static constexpr real s_coefs_low[10] = {
                -1.6989700043, -1.6989700043, -1.4779000000, -1.2291000000, -0.8648000000,
                -0.4480000000,  0.0051800000,  0.4511080334,  0.9113744414,  0.9113744414
            };
            static constexpr real s_coefs_high[10] = {
                 0.5154386965,  0.8470437783,  1.1358000000,  1.3802000000,  1.5197000000,
                 1.5985000000,  1.6467000000,  1.6746091357,  1.6878733390,  1.6878733390
            };

            static const real s_min_x = aces_spline_c5_fwd(0.18 * std::exp2(-6.5));
            static const real s_max_x = aces_spline_c5_fwd(0.18 * std::exp2(6.5));

            return aces_segmented_spline_eval(
                x,
                { s_min_x, 0.02 },
                { 4.8, 4.8 },
                { s_max_x, 48.0 },
                0.0, 0.04,
                s_coefs_low, s_coefs_high
            );
        }

        [[nodiscard]] auto tonemap_aces_full(const vec3& linear_rec709) noexcept -> vec3
        {
            auto const& cm = color_matrices();

            vec3 aces = cm.rec709_to_ap0 * glm::max(linear_rec709, vec3(0.0));

            real const saturation = aces_rgb_to_saturation(aces);
            real const yc_in = aces_rgb_to_yc(aces);
            real const s = aces_sigmoid_shaper((saturation - 0.4) / 0.2);
            aces *= (1.0 + aces_glow_fwd(yc_in, 0.05 * s, 0.08));

            real const hue = std::atan2(
                std::sqrt(3.0) * (aces.y - aces.z),
                2.0 * aces.x - aces.y - aces.z
            ) * (180.0 / std::numbers::pi_v<real>);
            real const f_H = aces_cubic_basis_shaper(hue, 135.0);
            if (f_H > 0.0) {
                aces.x += f_H * s * (0.03 - aces.x) * (1.0 - 0.82);
            }

            vec3 rgb_pre = glm::max(cm.ap0_to_ap1 * glm::max(aces, vec3(0.0)), vec3(0.0));
            rgb_pre = cm.rrt_sat * rgb_pre;

            vec3 const rgb_post(
                aces_spline_c5_fwd(rgb_pre.x),
                aces_spline_c5_fwd(rgb_pre.y),
                aces_spline_c5_fwd(rgb_pre.z)
            );

            vec3 rgb_odt(
                aces_spline_c9_fwd(rgb_post.x),
                aces_spline_c9_fwd(rgb_post.y),
                aces_spline_c9_fwd(rgb_post.z)
            );

            vec3 xyz = cm.ap1_to_xyz * rgb_odt;
            if (xyz.y > 1e-12) {
                xyz *= std::pow(xyz.y, 0.9811 - 1.0);
            }
            rgb_odt = cm.xyz_to_ap1 * xyz;

            rgb_odt = cm.odt_sat * rgb_odt;
            vec3 linear_out = cm.xyz_to_rec709 * (cm.ap1_to_xyz_d65 * rgb_odt);

            constexpr real cinema_black = 0.02;
            constexpr real cinema_white = 48.0;
            linear_out = (linear_out - cinema_black) / (cinema_white - cinema_black);

            return glm::max(linear_out, vec3(0.0));
        }

        [[nodiscard]] inline auto linear_srgb_to_oklab(const vec3& rgb) noexcept -> vec3
        {
            static const mat3 s_m1 = mat3_rows(
                0.4122214708, 0.5363325363, 0.0514459929,
                0.2119034982, 0.6806995451, 0.1073969566,
                0.0883024619, 0.2817188376, 0.6299787005
            );

            static const mat3 s_m2 = mat3_rows(
                0.2104542553,  0.7936177850, -0.0040720468,
                1.9779984951, -2.4285922050,  0.4505937099,
                0.0259040371,  0.7827717662, -0.8086757660
            );

            vec3 const lms = s_m1 * rgb;
            vec3 const lms_cbrt(std::cbrt(lms.x), std::cbrt(lms.y), std::cbrt(lms.z));

            return s_m2 * lms_cbrt;
        }

        [[nodiscard]] inline auto oklab_to_linear_srgb(const vec3& lab) noexcept -> vec3
        {
            static const mat3 s_inv_m2 = mat3_rows(
                1.0,  0.3963377774,  0.2158037573,
                1.0, -0.1055613458, -0.0638541728,
                1.0, -0.0894841775, -1.2914855480
            );

            static const mat3 s_inv_m1 = mat3_rows(
                 4.0767416621, -3.3077115913,  0.2309699292,
                -1.2684380046,  2.6097574011, -0.3413193965,
                -0.0041960863, -0.7034186147,  1.7076147010
            );

            vec3 const lms_cbrt = s_inv_m2 * lab;

            return s_inv_m1 * (lms_cbrt * lms_cbrt * lms_cbrt);
        }

        [[nodiscard]] auto gamut_map_oklch(const vec3& rgb) noexcept -> vec3
        {
            auto in_gamut = [](const vec3& c) noexcept -> bool {
                constexpr real eps = 1e-12;

                return c.x >= -eps && c.x <= 1.0 + eps &&
                       c.y >= -eps && c.y <= 1.0 + eps &&
                       c.z >= -eps && c.z <= 1.0 + eps;
            };

            if (in_gamut(rgb)) {
                return glm::clamp(rgb, vec3(0.0), vec3(1.0));
            }

            vec3 const lab = linear_srgb_to_oklab(glm::max(rgb, vec3(0.0)));
            if (lab.x >= 1.0) return vec3(1.0);
            if (lab.x <= 0.0) return vec3(0.0);

            real const chroma = std::hypot(lab.y, lab.z);
            if (chroma < 1e-12) {
                return glm::clamp(oklab_to_linear_srgb({ lab.x, 0.0, 0.0 }), vec3(0.0), vec3(1.0));
            }

            real const dir_a = lab.y / chroma;
            real const dir_b = lab.z / chroma;

            real low = 0.0;
            real high = chroma;
            for (int iter = 0; iter < 40; ++iter) {
                real const mid = 0.5 * (low + high);
                if (in_gamut(oklab_to_linear_srgb({ lab.x, mid * dir_a, mid * dir_b }))) {
                    low = mid;
                } else {
                    high = mid;
                }
            }

            return glm::clamp(oklab_to_linear_srgb({ lab.x, low * dir_a, low * dir_b }), vec3(0.0), vec3(1.0));
        }

        [[nodiscard]] inline auto linear_to_srgb_exact(real u) noexcept -> real
        {
            u = std::clamp(u, 0.0, 1.0);
            return (u <= 0.0031308) ? (12.92 * u) : (1.055 * std::pow(u, 1.0 / 2.4) - 0.055);
        }

        [[nodiscard]] inline auto linear_to_srgb_exact(const vec3& rgb) noexcept -> vec3
        {
            return {
                linear_to_srgb_exact(rgb.x),
                linear_to_srgb_exact(rgb.y),
                linear_to_srgb_exact(rgb.z)
            };
        }

        [[nodiscard]] constexpr auto splitmix64(u64 x) noexcept -> u64
        {
            x += 0x9e3779b97f4a7c15ULL;
            x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;

            return x ^ (x >> 31U);
        }

        [[nodiscard]] constexpr auto tpdf_dither(usize x, usize y, usize channel) noexcept -> real
        {
            u64 const key = (static_cast<u64>(x) << 32U) ^ (static_cast<u64>(y) << 4U) ^ static_cast<u64>(channel);
            u64 const h1 = splitmix64(key);
            u64 const h2 = splitmix64(h1);

            real const u1 = static_cast<real>(h1 >> 11U) * 0x1.0p-53;
            real const u2 = static_cast<real>(h2 >> 11U) * 0x1.0p-53;

            return (u1 - u2) / 255.0;
        }

    }

    auto PixelAccumulator::add_weighted(const vec3& L, real w) noexcept -> void
    {
        neumaier_add(m_weighted_sum, m_sum_compensation, L * w);
        neumaier_add(m_weight_sum, m_weight_compensation, w);
    }

    auto PixelAccumulator::add_welford(const vec3& L) noexcept -> void
    {
        ++m_sample_count;
        vec3 const delta = L - m_mean;
        m_mean += delta / static_cast<real>(m_sample_count);
        vec3 const delta2 = L - m_mean;
        m_m2 += delta * delta2;
    }

    auto PixelAccumulator::merge(const PixelAccumulator& other) noexcept -> void
    {
        neumaier_add(m_weighted_sum, m_sum_compensation,    other.weighted_sum());
        neumaier_add(m_weighted_sum, m_sum_compensation,    other.sum_compensation());
        neumaier_add(m_weight_sum,   m_weight_compensation, other.weight_sum());
        neumaier_add(m_weight_sum,   m_weight_compensation, other.weight_compensation());

        if (other.sample_count() > 0) {
            if (m_sample_count == 0) {
                m_sample_count = other.sample_count();
                m_mean = other.mean();
                m_m2 = other.m2();
            } else {
                u64 const n_a = m_sample_count;
                u64 const n_b = other.sample_count();
                u64 const n = n_a + n_b;
                vec3 const delta = other.mean() - m_mean;
                m_mean += delta * (static_cast<real>(n_b) / static_cast<real>(n));
                m_m2 += other.m2() + delta * delta * (static_cast<real>(n_a) * static_cast<real>(n_b) / static_cast<real>(n));
                m_sample_count = n;
            }
        }
    }

    [[nodiscard]] auto PixelAccumulator::resolved_radiance() const noexcept -> vec3
    {
        real const total_w = m_weight_sum + m_weight_compensation;
        if (std::abs(total_w) <= 1e-15) return vec3(0.0);
        return glm::max((m_weighted_sum + m_sum_compensation) / total_w, vec3(0.0));
    }

    [[nodiscard]] auto PixelAccumulator::standard_error() const noexcept -> vec3
    {
        if (m_sample_count < 2) return vec3(0.0);
        vec3 const var = glm::max(m_m2 / static_cast<real>(m_sample_count - 1), vec3(0.0));
        return glm::sqrt(var / static_cast<real>(m_sample_count));
    }

    FilmTile::FilmTile(usize width, usize height, usize x0, usize y0, usize x1, usize y1)
    {
        i64 const px0 = std::max<i64>(0, static_cast<i64>(std::ceil(static_cast<real>(x0) - 0.5 - s_filter_radius)));
        i64 const py0 = std::max<i64>(0, static_cast<i64>(std::ceil(static_cast<real>(y0) - 0.5 - s_filter_radius)));
        i64 const px1 = std::min<i64>(static_cast<i64>(width),  static_cast<i64>(std::floor(static_cast<real>(x1) - 0.5 + s_filter_radius)) + 1);
        i64 const py1 = std::min<i64>(static_cast<i64>(height), static_cast<i64>(std::floor(static_cast<real>(y1) - 0.5 + s_filter_radius)) + 1);

        m_x0 = static_cast<usize>(px0);
        m_y0 = static_cast<usize>(py0);
        m_x1 = static_cast<usize>(std::max(px0, px1));
        m_y1 = static_cast<usize>(std::max(py0, py1));

        m_width  = m_x1 - m_x0;
        m_height = m_y1 - m_y0;
        m_pixels.resize(m_width * m_height);
    }

    auto FilmTile::add_sample(const vec2& p_film, const vec3& radiance) noexcept -> void
    {
        if (!std::isfinite(radiance.x) || !std::isfinite(radiance.y) || !std::isfinite(radiance.z)) {
            return;
        }

        vec3 const L = glm::max(radiance, vec3(0.0));

        i64 const orig_x = static_cast<i64>(std::floor(p_film.x));
        i64 const orig_y = static_cast<i64>(std::floor(p_film.y));
        if (orig_x >= static_cast<i64>(m_x0) && orig_x < static_cast<i64>(m_x1) &&
            orig_y >= static_cast<i64>(m_y0) && orig_y < static_cast<i64>(m_y1)) {
            usize const idx = static_cast<usize>(orig_x - static_cast<i64>(m_x0)) +
                              static_cast<usize>(orig_y - static_cast<i64>(m_y0)) * m_width;
            m_pixels[idx].add_welford(L);
        }

        vec2 const p_discrete = p_film - 0.5;
        i64 const x_min = std::max<i64>(static_cast<i64>(m_x0), static_cast<i64>(std::ceil(p_discrete.x - s_filter_radius)));
        i64 const x_max = std::min<i64>(static_cast<i64>(m_x1) - 1, static_cast<i64>(std::floor(p_discrete.x + s_filter_radius)));
        i64 const y_min = std::max<i64>(static_cast<i64>(m_y0), static_cast<i64>(std::ceil(p_discrete.y - s_filter_radius)));
        i64 const y_max = std::min<i64>(static_cast<i64>(m_y1) - 1, static_cast<i64>(std::floor(p_discrete.y + s_filter_radius)));

        for (i64 y = y_min; y <= y_max; ++y) {
            real const dy = static_cast<real>(y) - p_discrete.y;
            usize const row_offset = static_cast<usize>(y - static_cast<i64>(m_y0)) * m_width;
            for (i64 x = x_min; x <= x_max; ++x) {
                real const dx = static_cast<real>(x) - p_discrete.x;
                real const w = evaluate_filter(dx, dy);
                if (w > 0.0) {
                    m_pixels[static_cast<usize>(x - static_cast<i64>(m_x0)) + row_offset].add_weighted(L, w);
                }
            }
        }
    }

    Film::Film(usize width, usize height, real exposure_ev)
        : m_width(width)
        , m_height(height)
        , m_exposure_scale(std::exp2(exposure_ev))
    {
        m_pixels.resize(m_width * m_height);
    }

    [[nodiscard]] auto Film::create_tile(usize x0, usize y0, usize x1, usize y1) const -> FilmTile
    {
        return FilmTile(m_width, m_height, x0, y0, std::min(x1, m_width), std::min(y1, m_height));
    }

    auto Film::merge_tile(const FilmTile& tile) -> void
    {
        std::lock_guard lock(m_mutex);
        auto const& tile_pixels = tile.pixels();
        usize const tw = tile.width();

        for (usize ty = 0; ty < tile.height(); ++ty) {
            usize const fy = tile.y0() + ty;
            for (usize tx = 0; tx < tw; ++tx) {
                usize const fx = tile.x0() + tx;
                m_pixels[fx + fy * m_width].merge(tile_pixels[tx + ty * tw]);
            }
        }
    }

    auto Film::set(usize x, usize y, const vec3& rgb) -> void
    {
        set(x, y, vec4(rgb, 1.0));
    }

    auto Film::set(usize x, usize y, const vec4& rgba) -> void
    {
        if (x >= m_width || y >= m_height) return;
        std::lock_guard lock(m_mutex);
        PixelAccumulator acc {};
        vec3 const L = glm::max(vec3(rgba.x, rgba.y, rgba.z), vec3(0.0));
        acc.add_weighted(L, rgba.w);
        acc.add_welford(L);
        m_pixels[x + y * m_width] = acc;
    }

    [[nodiscard]] auto Film::develop(std::vector<vec3>* out_linear_hdr) const -> std::vector<vec3>
    {
        usize const num_pixels = m_width * m_height;
        std::vector<vec3> hdr(num_pixels, vec3(0.0));

        for (usize i = 0; i < num_pixels; ++i) {
            hdr[i] = m_pixels[i].resolved_radiance() * m_exposure_scale;
        }

        if (m_width >= 3 && m_height >= 3) {
            std::vector<vec3> filtered = hdr;
            std::vector<real> neighbor_lums;
            neighbor_lums.reserve(8);

            for (usize y = 0; y < m_height; ++y) {
                for (usize x = 0; x < m_width; ++x) {
                    usize const idx = x + y * m_width;
                    real const center_lum = luminance(hdr[idx]);
                    if (center_lum <= 1e-6) continue;

                    real const pixel_sem = luminance(m_pixels[idx].standard_error()) * m_exposure_scale;
                    if (m_pixels[idx].sample_count() >= 2 && pixel_sem < 1e-3) {
                        continue; // Converged highlight
                    }

                    neighbor_lums.clear();
                    for (i64 dy = -1; dy <= 1; ++dy) {
                        for (i64 dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            i64 const nx = static_cast<i64>(x) + dx;
                            i64 const ny = static_cast<i64>(y) + dy;
                            if (nx >= 0 && nx < static_cast<i64>(m_width) && ny >= 0 && ny < static_cast<i64>(m_height)) {
                                neighbor_lums.push_back(luminance(hdr[static_cast<usize>(nx) + static_cast<usize>(ny) * m_width]));
                            }
                        }
                    }

                    if (neighbor_lums.size() >= 3) {
                        std::sort(neighbor_lums.begin(), neighbor_lums.end());
                        real const median = neighbor_lums[neighbor_lums.size() / 2];

                        std::vector<real> abs_devs = neighbor_lums;
                        for (real& d : abs_devs) d = std::abs(d - median);
                        std::sort(abs_devs.begin(), abs_devs.end());

                        real const sigma_mad = 1.4826 * abs_devs[abs_devs.size() / 2] + 1e-4;
                        real const max_allowed = median + 4.0 * sigma_mad;

                        if (center_lum > max_allowed && center_lum > 4.0 * median) {
                            filtered[idx] = hdr[idx] * (max_allowed / center_lum);
                        }
                    }
                }
            }
            hdr = std::move(filtered);
        }

        if (out_linear_hdr != nullptr) {
            *out_linear_hdr = hdr;
        }

        std::vector<vec3> display_linear(num_pixels, vec3(0.0));
        for (usize i = 0; i < num_pixels; ++i) {
            display_linear[i] = gamut_map_oklch(tonemap_aces_full(hdr[i]));
        }

        return display_linear;
    }

    auto Film::save(const std::string& filename) const -> void
    {
        if (filename.ends_with(".hdr")) {
            std::vector<vec3> linear_hdr;
            (void)develop(&linear_hdr);

            std::vector<f32> float_buf(m_width * m_height * 3, 0.0f);
            for (usize i = 0; i < linear_hdr.size(); ++i) {
                float_buf[i * 3 + 0] = static_cast<f32>(linear_hdr[i].x);
                float_buf[i * 3 + 1] = static_cast<f32>(linear_hdr[i].y);
                float_buf[i * 3 + 2] = static_cast<f32>(linear_hdr[i].z);
            }

            stbi_write_hdr(
                filename.c_str(),
                static_cast<i32>(m_width),
                static_cast<i32>(m_height),
                3,
                float_buf.data()
            );
            return;
        }

        std::vector<vec3> const display_linear = develop();
        std::vector<u8> buffer(m_width * m_height * s_channels, 255);

        for (usize y = 0; y < m_height; ++y) {
            for (usize x = 0; x < m_width; ++x) {
                usize const idx = x + y * m_width;
                vec3 srgb = linear_to_srgb_exact(display_linear[idx]);

                srgb.x += tpdf_dither(x, y, 0);
                srgb.y += tpdf_dither(x, y, 1);
                srgb.z += tpdf_dither(x, y, 2);

                usize const out_idx = idx * s_channels;
                buffer[out_idx + 0] = static_cast<u8>(std::clamp(std::floor(srgb.x * 255.0 + 0.5), 0.0, 255.0));
                buffer[out_idx + 1] = static_cast<u8>(std::clamp(std::floor(srgb.y * 255.0 + 0.5), 0.0, 255.0));
                buffer[out_idx + 2] = static_cast<u8>(std::clamp(std::floor(srgb.z * 255.0 + 0.5), 0.0, 255.0));
                buffer[out_idx + 3] = 255;
            }
        }

        stbi_write_png(
            filename.c_str(),
            static_cast<i32>(m_width),
            static_cast<i32>(m_height),
            static_cast<i32>(s_channels),
            reinterpret_cast<const void*>(buffer.data()),
            static_cast<i32>(m_width * s_channels)
        );
    }

}
