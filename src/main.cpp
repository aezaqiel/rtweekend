#include <glm/glm.hpp>

#include "image.hpp"
#include "camera.hpp"

#include "hittable/hittable_list.hpp"
#include "hittable/sphere.hpp"

#include "sampler/independentsampler.hpp"

#include "progress.hpp"

using namespace rtweekend;

auto trace(const Ray& ray, const Hittable& world) -> glm::vec3
{
    HitRecord record;
    if (world.hit(ray, Interval { 0.001f, std::numeric_limits<f32>::infinity() }, record)) {
        return 0.5f * (record.normal + 1.0f);
    }

    glm::vec3 dir = glm::normalize(ray.direction());
    f32 a = 0.5f * (dir.y + 1.0f);

    return glm::mix(glm::vec3(1.0f), glm::vec3(0.5f, 0.7f, 1.0f), a);
}

auto singular_sphere() -> HittableList
{
    HittableList world;
    world.push(std::make_shared<Sphere>(glm::vec3(0.0f, 0.0f, -1.0f), 0.5f));
    world.push(std::make_shared<Sphere>(glm::vec3(0.0f, -100.5f, -1.0f), 100.0f));

    return world;
}

auto main() -> int
{
    usize image_width = 1280;
    usize image_height = 720;
    usize image_samples = 32;

    Image image(image_width, image_height);
    Camera camera(image_width, image_height, glm::vec3(0.0f));

    IndependentSampler sampler;

    HittableList world;
    world = singular_sphere();

    ProgressBar progress(image_height);

    for (usize j = 0; j < image_height; ++j) {
        for (usize i = 0; i < image_width; ++i) {
            glm::vec3 sample(0.0f);

            for (usize s = 0; s < image_samples; ++s) {
                sampler.seed_pixel(i, j, s);
                glm::vec2 offset = sampler.sample_2d();

                Ray ray = camera(i, j, offset);
                sample += trace(ray, world);
            }

            image.set(i, j, sample / static_cast<f32>(image_samples));
        }

        progress.update();
    }
    progress.finish();

    image.save("image.png");
}
