#include <print>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include "image.hpp"

#include "sampler/independent_sampler.hpp"
#include "camera/thinlens_camera.hpp"
#include "integrator/path_integrator.hpp"

#include "hittable/hittable_list.hpp"
#include "hittable/sphere.hpp"

#include "material/lambertian.hpp"
#include "material/metal.hpp"
#include "material/dielectric.hpp"

using namespace rtweekend;

auto spheres() -> HittableList
{
    auto ground_material = std::make_shared<Lambertian>(glm::vec3(0.8f, 0.8f, 0.2f));
    auto center_material = std::make_shared<Dielectric>(1.5f);
    auto left_material = std::make_shared<Metal>(glm::vec3(0.8f, 0.8f, 0.8f));
    auto right_material = std::make_shared<Metal>(glm::vec3(0.8f, 0.6f, 0.2f), 0.5f);

    HittableList world;
    world.push(std::make_shared<Sphere>(glm::vec3( 0.0f, -100.5f, -1.0f), 100.0f, ground_material));
    world.push(std::make_shared<Sphere>(glm::vec3( 0.0f,    0.0f, -1.2f),   0.5f, center_material));
    world.push(std::make_shared<Sphere>(glm::vec3(-1.0f,    0.0f, -1.0f),   0.5f, left_material));
    world.push(std::make_shared<Sphere>(glm::vec3( 1.0f,    0.0f, -1.0f),   0.5f, right_material));

    return world;
}

auto book1(Sampler& sampler) -> HittableList
{
    HittableList world;

    auto ground_material = std::make_shared<Lambertian>(glm::vec3(0.5f, 0.5f, 0.5f));
    world.push(std::make_shared<Sphere>(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, ground_material));

    for (i32 a = -11; a < 11; ++a) {
        for (i32 b = -11; b < 11; ++b) {
            f32 const choose_mat = sampler.sample_1d();
            glm::vec3 const center(
                static_cast<f32>(a) + 0.9f * sampler.sample_1d(),
                0.2f,
                static_cast<f32>(b) + 0.9f * sampler.sample_1d()
            );

            if (glm::length(center - glm::vec3(4.0f, 0.2f, 0.0f)) > 0.9f) {
                std::shared_ptr<Material> sphere_material;

                if (choose_mat < 0.8f) {
                    glm::vec3 const c0(sampler.sample_1d(), sampler.sample_1d(), sampler.sample_1d());
                    glm::vec3 const c1(sampler.sample_1d(), sampler.sample_1d(), sampler.sample_1d());
                    glm::vec3 const albedo = c0 * c1;
                    sphere_material = std::make_shared<Lambertian>(albedo);
                } else if (choose_mat < 0.95f) {
                    glm::vec3 const albedo(
                        sampler.uniform(0.5f, 1.0f),
                        sampler.uniform(0.5f, 1.0f),
                        sampler.uniform(0.5f, 1.0f)
                    );
                    f32 const fuzz = sampler.uniform(0.0f, 0.5f);
                    sphere_material = std::make_shared<Metal>(albedo, fuzz);
                } else {
                    sphere_material = std::make_shared<Dielectric>(1.5f);
                }

                world.push(std::make_shared<Sphere>(center, 0.2f, sphere_material));
            }
        }
    }

    auto material1 = std::make_shared<Dielectric>(1.5f);
    world.push(std::make_shared<Sphere>(glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, material1));

    auto material2 = std::make_shared<Lambertian>(glm::vec3(0.4f, 0.2f, 0.1f));
    world.push(std::make_shared<Sphere>(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, material2));

    auto material3 = std::make_shared<Metal>(glm::vec3(0.7f, 0.6f, 0.5f), 0.0f);
    world.push(std::make_shared<Sphere>(glm::vec3(4.0f, 1.0f, 0.0f), 1.0f, material3));

    return world;
}

auto main() -> int
{
    usize const image_width = 3840;
    usize const image_height = 2160;
    usize const image_samples = 1024;
    usize const image_depth = 8;

    std::string const output_filename = "image.png";

    Image image(image_width, image_height);

    // ThinLensCamera::Config camera_config {
    //     .width = image_width,
    //     .height = image_height,
    //     .vfov = 90.0f,
    //     .lookfrom = { 0.0f, 0.0f, 0.0f },
    //     .lookat = { 0.0f, 0.0f, -1.0f },
    //     .vup = { 0.0f, 1.0f, 0.0f },
    //     .defocus_angle = 0.0f,
    //     .focal_length = 1.0f
    // };

    ThinLensCamera::Config camera_config {
        .width = image_width,
        .height = image_height,
        .vfov = 20.0f,
        .lookfrom = { 13.0f, 2.0f, 3.0f },
        .lookat = { 0.0f, 0.0f, 0.0f },
        .vup = { 0.0f, 1.0f, 0.0f },
        .defocus_angle = 0.6f,
        .focal_length = 10.0f
    };

    ThinLensCamera camera(camera_config);

    auto sampler = std::make_unique<IndependentSampler>();

    HittableList world;
    // world = spheres();
    world = book1(*sampler);

    // pre-render diagnostics
    usize const total_pixels = image_width * image_height;
    u64 const total_samples = static_cast<u64>(total_pixels) * static_cast<u64>(image_samples);

    std::print("\033[1;36m=== Render Configuration ===\033[0m\n");
    std::print("  Resolution      : {} x {} ({} pixels)\n", image_width, image_height, total_pixels);
    std::print("  Samples         : {}\n", image_samples);
    std::print("  Max Depth       : {}\n", image_depth);
    std::print("  Camera FOV      : {:.1f} deg\n", camera_config.vfov);
    std::print("  Look From -> At : {} -> {}\n", glm::to_string(camera_config.lookfrom), glm::to_string(camera_config.lookat));
    std::print("  Focus / Defocus : focal = {:.1f}, angle = {:.2f} deg\n\n", camera_config.focal_length, camera_config.defocus_angle);

    PathIntegrator integrator(std::move(sampler), image_samples, image_depth);

    auto const start_time = std::chrono::steady_clock::now();
    integrator.render(world, camera, image);
    auto const end_time = std::chrono::steady_clock::now();

    f64 const elapsed_sec = std::chrono::duration<f64>(end_time - start_time).count();
    f64 const msamples_per_sec = (static_cast<f64>(total_samples) / 1e6) / std::max(elapsed_sec, 1e-9);

    image.save(output_filename);

    // post-render diagnostics
    std::print("\n\033[1;32m=== Render Complete ===\033[0m\n");
    std::print("  Render Time    : {:.2f}s\n", elapsed_sec);
    std::print("  Throughput     : {:.2f} MSamples/s ({:.0f} pixels/s)\n", msamples_per_sec, static_cast<f64>(total_pixels) / std::max(elapsed_sec, 1e-9));
    std::print("  Saved Output   : {}\n", output_filename);
}
