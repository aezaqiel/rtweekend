#include "bvh.hpp"

#include <numeric>
#include <algorithm>

namespace rtweekend {

    BVH::BVH(const HittableList& list)
        : BVH(list.objects())
    {
    }

    BVH::BVH(std::vector<std::shared_ptr<Hittable>> primitives)
        : m_primitives(std::move(primitives))
    {
        for (const auto& p : m_primitives) {
            p->collect_emitters(m_emitters);
        }

        if (!m_primitives.empty()) {
            m_nodes.reserve(2 * m_primitives.size());
            (void)build_recursive(0, m_primitives.size());
        }
    }

    auto BVH::hit(const Ray& ray, Interval t, HitRecord& record) const -> bool
    {
        if (m_nodes.empty()) return false;

        vec3 const d = ray.direction();
        vec3 const inv_dir = 1.0 / d;
        bool const dir_is_neg[3] = {inv_dir.x < 0.0, inv_dir.y < 0.0, inv_dir.z < 0.0 };

        usize stack[64];
        usize stack_ptr = 0;
        usize current = 0;

        bool hit_anything = false;
        real closest = t.max;
        HitRecord scratch;

        while (true) {
            const LinearNode& node = m_nodes[current];

            if (node.bbox.hit(ray, inv_dir, Interval { t.min, closest })) {
                if (node.count > 0) {
                    for (usize i = 0; i < node.count; ++i) {
                        if (m_primitives[node.offset + i]->hit(ray, Interval { t.min, closest }, scratch)) {
                            hit_anything = true;
                            closest = scratch.distance;
                            record = scratch;
                        }
                    }

                    if (stack_ptr == 0) break;
                    current = stack[--stack_ptr];
                } else {
                    usize const left_child = current + 1;
                    usize const right_child = node.offset;

                    if (dir_is_neg[node.axis]) {
                        stack[stack_ptr++] = left_child;
                        current = right_child;
                    } else {
                        stack[stack_ptr++] = right_child;
                        current = left_child;
                    }
                }
            } else {
                if (stack_ptr == 0) break;
                current = stack[--stack_ptr];
            }
        }

        return hit_anything;
    }

    [[nodiscard]] auto BVH::bounding_box() const noexcept -> AABB
    {
        return m_nodes.empty() ? AABB::empty() : m_nodes[0].bbox;
    }

    [[nodiscard]] auto BVH::sample_solid_angle(const vec3& origin, real u_select, const vec2& u_dir) const -> std::optional<LightSample>
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

    [[nodiscard]] auto BVH::pdf_solid_angle(const vec3& origin, const vec3& wi) const -> real
    {
        if (m_emitters.empty()) return 0.0;

        real sum_pdf = 0.0;
        for (const Hittable* emitter : m_emitters) {
            sum_pdf += emitter->pdf_solid_angle(origin, wi);
        }

        return sum_pdf / static_cast<real>(m_emitters.size());
    }

    auto BVH::collect_emitters(std::vector<const Hittable*>& out) const -> void
    {
        out.insert(out.end(), m_emitters.begin(), m_emitters.end());
    }

    auto BVH::build_recursive(usize start, usize end) -> usize
    {
        usize const node_idx = m_nodes.size();
        m_nodes.push_back(LinearNode {});

        AABB bounds = AABB::empty();
        for (usize i = start; i < end; ++i) {
            bounds = AABB::merge(bounds, m_primitives[i]->bounding_box());
        }

        usize const span = end - start;
        if (span <= 2) {
            m_nodes[node_idx] = LinearNode {
                .bbox = bounds,
                .offset = start,
                .count = static_cast<u16>(span),
                .axis = 0
            };

            return node_idx;
        }

        real const parent_area = bounds.surface_area();
        constexpr real traversal_cost = 0.5;
        real best_cost = static_cast<real>(span);

        i32 best_axis = -1;
        usize best_split = start + span / 2;

        if (parent_area > 1e-16) {
            std::vector<usize> indices(span);
            std::vector<AABB> suffix_boxes(span);

            for (i32 axis = 0; axis < 3; ++axis) {
                std::iota(indices.begin(), indices.end(), start);
                std::sort(indices.begin(), indices.end(), [&](usize a, usize b) {
                    return m_primitives[a]->bounding_box().centroid()[axis]
                        < m_primitives[b]->bounding_box().centroid()[axis];
                });

                AABB accum_right = AABB::empty();
                for (usize i = span; i-- > 0;) {
                    accum_right = AABB::merge(accum_right, m_primitives[indices[i]]->bounding_box());
                    suffix_boxes[i] = accum_right;
                }

                AABB accum_left = AABB::empty();
                for (usize i = 0; i < span - 1; ++i) {
                    accum_left = AABB::merge(accum_left, m_primitives[indices[i]]->bounding_box());
                    real const left_count = static_cast<real>(i + 1);
                    real const right_count = static_cast<real>(span - 1 - i);
                    real const cost = traversal_cost
                        + (accum_left.surface_area() * left_count + suffix_boxes[i + 1].surface_area() * right_count)
                        / parent_area;

                    if (cost < best_cost) {
                        best_cost = cost;
                        best_axis = axis;
                        best_split = start + i + 1;
                    }
                }
            }
        }

        if (best_axis == -1) {
            if (span <= 8) {
                m_nodes[node_idx] = LinearNode {
                    .bbox = bounds,
                    .offset = start,
                    .count = static_cast<u16>(span),
                    .axis = 0
                };

                return node_idx;
            }

            vec3 const diag = bounds.diagonal();
            best_axis = (diag.x >= diag.y && diag.x >= diag.z) ? 0 : ((diag.y >= diag.z) ? 1 : 2);
            best_split = start + span / 2;
        }

        std::nth_element(
            m_primitives.begin() + static_cast<std::ptrdiff_t>(start),
            m_primitives.begin() + static_cast<std::ptrdiff_t>(best_split),
            m_primitives.begin() + static_cast<std::ptrdiff_t>(end),
            [best_axis](const auto& a, const auto& b) {
                return a->bounding_box().centroid()[best_axis]
                    < b->bounding_box().centroid()[best_axis];
            }
        );

        (void)build_recursive(start, best_split);
        usize const right_child_idx = build_recursive(best_split, end);

        m_nodes[node_idx] = LinearNode {
            .bbox = bounds,
            .offset = right_child_idx,
            .count = 0,
            .axis = static_cast<u8>(best_axis)
        };

        return node_idx;
    }

}
