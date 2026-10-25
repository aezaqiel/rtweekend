#include <glm/glm.hpp>

#include "image.hpp"

#include "sampler/independent_sampler.hpp"
#include "camera/thinlens_camera.hpp"
#include "integrator/path_integrator.hpp"

#include "hittable/hittable_list.hpp"
#include "hittable/sphere.hpp"

using namespace rtweekend;

auto singular_sphere() -> HittableList
{
    HittableList world;
    world.push(std::make_shared<Sphere>(glm::vec3(0.0f, 0.0f, -1.0f), 0.5f));
    world.push(std::make_shared<Sphere>(glm::vec3(0.0f, -100.5f, -1.0f), 100.0f));

    return world;
}

auto main() -> int
{
    usize image_width = 400;
    usize image_height = 300;
    usize image_samples = 16;
    usize image_depth = 8;

    Image image(image_width, image_height);

    ThinLensCamera::Config camera_config {
        .width = image_width,
        .height = image_height,
        .vfov = 90.0f,
        .lookfrom = { 0.0f, 0.0f, 0.0f },
        .lookat = { 0.0f, 0.0f, -1.0f },
        .vup = { 0.0f, 1.0f, 0.0f },
        .defocus_angle = 0.0f,
        .focal_length = 1.0f
    };

    ThinLensCamera camera(camera_config);

    HittableList world;
    world = singular_sphere();

    auto sampler = std::make_unique<IndependentSampler>();

    PathIntegrator integrator(std::move(sampler), image_samples, image_depth);
    integrator.render(world, camera, image);

    image.save("image.png");
}
