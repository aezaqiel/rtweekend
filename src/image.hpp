#pragma once

#include <string>

#include <glm/glm.hpp>

namespace rtweekend {

    class Image
    {
    public:
        Image(usize width, usize height);
        ~Image() = default;

        inline auto width() const -> usize { return m_width; }
        inline auto height() const -> usize { return m_height; }
        inline auto channels() const -> usize { return s_channels; }

        auto set(usize x, usize y, const glm::vec3& rgb) -> void;

        auto save(const std::string& filename) -> void;

    private:
        usize m_width { 0 };
        usize m_height { 0 };

        inline static constexpr usize s_channels { 3 };

        std::vector<u8> m_buffer;
    };

}
