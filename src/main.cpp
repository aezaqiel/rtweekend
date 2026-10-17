
#include "image.hpp"

auto main() -> int
{
    using namespace rtweekend;

    usize image_width = 1280;
    usize image_height = 720;

    Image image(image_width, image_height);

    for (usize j = 0; j < image_height; ++j) {
        for (usize i = 0; i < image_width; ++i) {
            f32 u = static_cast<f32>(i) / static_cast<f32>(image_width);
            f32 v = static_cast<f32>(j) / static_cast<f32>(image_height);
            f32 w = 0.25f;

            glm::vec3 rgb(u, v, w);
            image.set(i, j, rgb);
        }
    }

    image.save("image.png");
}
