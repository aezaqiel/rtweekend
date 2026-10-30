#pragma once

#include "material.hpp"

namespace rtweekend {

    class PrincipledBSDF final : public Material
    {
    public:
        struct Config
        {
            vec3 base_color             { 0.8, 0.8, 0.8 };
            real metallic               { 0.0  };
            real roughness              { 0.5  };
            real subsurface             { 0.0  };
            real specular               { 0.5  };
            real specular_tint          { 0.0  };
            real anisotropic            { 0.0  };
            real sheen                  { 0.0  };
            real sheen_tint             { 0.5  };
            real clearcoat              { 0.0  };
            real clearcoat_roughness    { 0.03 };
            real clearcoat_ior          { 1.5  };
            real ior                    { 1.5  };
            real spec_trans             { 0.0  };
            real attenuation_distance   { 1.0  };
            vec3 emission               { 0.0, 0.0, 0.0 };
        };
    
    public:
        explicit PrincipledBSDF(const Config& config);
        virtual ~PrincipledBSDF() = default;

        [[nodiscard]] static auto diffuse(const vec3& base_color, real roughness = 0.5, real subsurface = 0.0) -> std::shared_ptr<PrincipledBSDF>;
        [[nodiscard]] static auto metal(const vec3& base_color, real roughness = 0.0, real anisotropic = 0.0) -> std::shared_ptr<PrincipledBSDF>;
        [[nodiscard]] static auto dielectric(real ior = 1.5, real roughness = 0.0, const vec3& base_color = vec3(1.0), real attenuation_distance = 1.0) -> std::shared_ptr<PrincipledBSDF>;
        [[nodiscard]] static auto plastic(const vec3& base_color, real roughness = 0.15, real clearcoat = 0.5) -> std::shared_ptr<PrincipledBSDF>;
        [[nodiscard]] static auto emissive(const vec3& radiance) -> std::shared_ptr<PrincipledBSDF>;

        [[nodiscard]] virtual auto sample(const Ray& ray, const HitRecord& record, const MaterialSample& sample) const -> std::optional<BSDFSample> override;
        [[nodiscard]] virtual auto eval(const vec3& wo_world, const vec3& wi_world, const HitRecord& record) const -> vec3 override;
        [[nodiscard]] virtual auto pdf(const vec3& wo_world, const vec3& wi_world, const HitRecord& record) const -> real override;

        [[nodiscard]] virtual auto emission(const HitRecord& record) const noexcept -> vec3 override
        {
            return m_emission;
        }

        [[nodiscard]] virtual auto interior_sigma_a() const noexcept -> vec3 override
        {
            return m_sigma_a;
        }

    private:
        struct LobeWeights
        {
            real diffuse    { 0.0 };
            real microfacet { 0.0 };
            real clearcoat  { 0.0 };
        };

    private:
        [[nodiscard]] auto compute_lobe_weights(real cos_theta_0, real eta) const noexcept -> LobeWeights;
        [[nodiscard]] auto transmission_prob(real cos_theta_d, real eta) const noexcept -> real;
        [[nodiscard]] auto specular_fresnel(real cos_theta_d, real eta) const noexcept -> vec3;

        [[nodiscard]] auto eval_local(const vec3& wo, const vec3& wi, real eta) const noexcept -> vec3;
        [[nodiscard]] auto pdf_local(const vec3& wo, const vec3& wi, real eta) const noexcept -> real;

    private:
        vec3 m_base_color           { 0.8, 0.8, 0.8 };
        real m_metallic             { 0.0  };
        real m_roughness            { 0.5  };
        real m_subsurface           { 0.0  };
        real m_specular             { 0.5  };
        real m_specular_tint        { 0.0  };
        real m_anisotropic          { 0.0  };
        real m_sheen                { 0.0  };
        real m_sheen_tint           { 0.5  };
        real m_clearcoat            { 0.0  };
        real m_clearcoat_roughness  { 0.03 };
        real m_clearcoat_ior        { 1.5  };
        real m_ior                  { 1.5  };
        real m_spec_trans           { 0.0  };
        vec3 m_emission             { 0.0, 0.0, 0.0 };

        bool m_is_delta_mf          { false };
        real m_alpha_x              { 0.25  };
        real m_alpha_y              { 0.25  };
        real m_alpha_coat           { 0.001 };
        real m_opaque_ior           { 1.5   };
        vec3 m_conductor_eta        { 1.0, 1.0, 1.0 };
        vec3 m_conductor_k          { 1.0, 1.0, 1.0 };
        vec3 m_sigma_a              { 0.0, 0.0, 0.0 };
        vec3 m_tint                 { 1.0, 1.0, 1.0 };
        vec3 m_spec_tint_color      { 1.0, 1.0, 1.0 };
        vec3 m_sheen_color          { 1.0, 1.0, 1.0 };
    };

}
