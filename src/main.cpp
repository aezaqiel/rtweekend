#include <glm/glm.hpp>

#include "image.hpp"
#include "camera.hpp"

using namespace rtweekend;

auto hit(const glm::vec3& center, f32 radius, const Ray& ray) -> bool
{
    glm::vec3 oc = center - ray.origin();

    f32 a = glm::dot(ray.direction(), ray.direction());
    f32 b = -2.0f * glm::dot(ray.direction(), oc);
    f32 c = glm::dot(oc, oc) - radius * radius;

    f32 discriminant = b * b - 4.0f * a * c;

    return (discriminant >= 0.0f);
}

auto trace(const Ray& ray) -> glm::vec3
{
    if (hit(glm::vec3(0.0f, 0.0f, -1.0f), 0.5f, ray)) {
        return glm::vec3(0.8f, 0.2f, 0.8f);
    }

    glm::vec3 dir = glm::normalize(ray.direction());
    f32 a = 0.5f * (dir.y + 1.0f);

    return glm::mix(glm::vec3(1.0f), glm::vec3(0.5f, 0.7f, 1.0f), a);
}

auto main() -> int
{
    usize image_width = 1280;
    usize image_height = 720;

    Image image(image_width, image_height);
    Camera camera(image_width, image_height, glm::vec3(0.0f));

    for (usize j = 0; j < image_height; ++j) {
        for (usize i = 0; i < image_width; ++i) {
            Ray ray = camera(i, j);
            glm::vec3 sample = trace(ray);

            image.set(i, j, sample);
        }
    }

    image.save("image.png");
}
