#include "hittable_list.hpp"

namespace rtweekend {

    auto HittableList::hit(const Ray& ray, Interval t, HitRecord& record) const -> bool
    {
        HitRecord scratch;
        bool hit = false;
        real closest = t.max;

        for (const auto& hittable : m_hittables) {
            if (hittable->hit(ray, Interval { t.min, closest }, scratch)) {
                hit = true;
                closest = scratch.distance;
                record = scratch;
            }
        }

        return hit;
    }

    [[nodiscard]] auto HittableList::sample_solid_angle(const vec3& origin, real u_select, const vec2& u_dir) const -> std::optional<LightSample>
    {
        if (m_emitters.empty()) return std::nullopt;

        usize const n = m_emitters.size();
        usize const idx = std::min(static_cast<usize>(u_select * static_cast<real>(n)), n - 1);

        auto sample = m_emitters[idx]->sample_solid_angle(origin, u_select, u_dir);
        if (!sample.has_value()) return std::nullopt;

        sample->pdf = pdf_solid_angle(origin, sample->wi);
        if (sample->pdf <= 0.0) return std::nullopt;

        return sample;
    }

    [[nodiscard]] auto HittableList::pdf_solid_angle(const vec3& origin, const vec3& wi) const -> real
    {
        if (m_emitters.empty()) return 0.0;

        real sum_pdf = 0.0;
        for (const Hittable* emitter : m_emitters) {
            sum_pdf += emitter->pdf_solid_angle(origin, wi);
        }

        return sum_pdf / static_cast<real>(m_emitters.size());
    }

    auto HittableList::collect_emitters(std::vector<const Hittable*>& out) const -> void
    {
        for (const auto& h : m_hittables) {
            h->collect_emitters(out);
        }
    }

}
