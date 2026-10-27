#pragma once

#include <cstdint>
#include <cstddef>

#include <glm/glm.hpp>

namespace rtweekend {

    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using f32 = float;
    using f64 = double;

    using usize = std::size_t;

    using real = f64;

    using vec2 = glm::dvec2;
    using vec3 = glm::dvec3;
    using vec4 = glm::dvec4;

}
