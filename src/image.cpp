#include "image.hpp"

#include <algorithm>

#include <stb_image_write.h>

namespace rtweekend {

    Image::Image(usize width, usize height)
        : m_width(width)
        , m_height(height)
    {
        m_buffer.resize(m_width * m_height * s_channels, 0);
    }

    auto Image::set(usize x, usize y, const vec3& rgb) -> void
    {
        set(x, y, vec4(rgb, 1.0));
    }

    auto Image::set(usize x, usize y, const vec4& rgba) -> void
    {
        usize index = (x + y * m_width) * s_channels;
        for (usize i = 0; i < s_channels; ++i) {
            real const c = std::clamp(rgba[static_cast<vec4::length_type>(i)], 0.0, 1.0);
            m_buffer[index + i] = static_cast<u8>(std::floor(c * 255.0 + 0.5));
        }
    }

    auto Image::save(const std::string& filename) -> void
    {
        stbi_write_png(
            filename.c_str(),
            static_cast<i32>(m_width),
            static_cast<i32>(m_height),
            static_cast<i32>(s_channels),
            reinterpret_cast<const void*>(m_buffer.data()),
            static_cast<i32>(m_width * s_channels)
        );
    }

}
