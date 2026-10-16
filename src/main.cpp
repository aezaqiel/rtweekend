#include <vector>

#include <stb_image_write.h>

auto main() -> int
{
    using namespace rtweekend;

    usize image_width = 1280;
    usize image_height = 720;

    std::vector<u8> framebuffer(image_width * image_height * 3, 0);

    for (usize j = 0; j < image_height; ++j) {
        for (usize i = 0; i < image_width; ++i) {
            f32 u = static_cast<f32>(i) / static_cast<f32>(image_width);
            f32 v = static_cast<f32>(j) / static_cast<f32>(image_height);
            f32 w = 0.25f;

            u8 r = static_cast<u8>(u * 255.0f);
            u8 g = static_cast<u8>(v * 255.0f);
            u8 b = static_cast<u8>(w * 255.0f);

            usize index = (i + j * image_width) * 3;
            framebuffer[index + 0] = r;
            framebuffer[index + 1] = g;
            framebuffer[index + 2] = b;
        }
    }

    stbi_write_png("image.png", static_cast<i32>(image_width), static_cast<i32>(image_height), 3, reinterpret_cast<const void*>(framebuffer.data()), static_cast<i32>(image_width * 3));
}
