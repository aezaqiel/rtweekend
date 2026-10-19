#pragma once

#include <vector>
#include <memory>

#include "hittable.hpp"

namespace rtweekend {

    class HittableList : public Hittable
    {
    public:
        HittableList() = default;
        virtual ~HittableList() = default;

        inline auto push(const std::shared_ptr<Hittable>& hittable) -> void
        {
            m_hittables.push_back(hittable);
        }

        virtual auto hit(const Ray& ray, Interval t, HitRecord& record) const -> bool override;

    private:
        std::vector<std::shared_ptr<Hittable>> m_hittables;
    };

}
