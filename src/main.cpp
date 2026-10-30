#include <print>
#include <chrono>

#include <glm/gtx/string_cast.hpp>

#include "sampler/independent_sampler.hpp"
#include "camera/thinlens_camera.hpp"
#include "integrator/path_integrator.hpp"

#include "hittable/hittable_list.hpp"
#include "hittable/sphere.hpp"

#include "material/principled.hpp"

using namespace rtweekend;

auto spheres() -> HittableList
{
    auto ground_material = PrincipledBSDF::diffuse(vec3(0.8, 0.8, 0.2), 0.8);
    auto center_material = PrincipledBSDF::dielectric(1.5);
    auto left_material   = PrincipledBSDF::metal(vec3(0.8, 0.8, 0.8), 0.0);
    auto right_material  = PrincipledBSDF::metal(vec3(0.8, 0.6, 0.2), 0.5);

    HittableList world;
    world.push(std::make_shared<Sphere>(vec3( 0.0, -100.5, -1.0), 100.0, ground_material));
    world.push(std::make_shared<Sphere>(vec3( 0.0,    0.0, -1.2),   0.5, center_material));
    world.push(std::make_shared<Sphere>(vec3(-1.0,    0.0, -1.0),   0.5, left_material));
    world.push(std::make_shared<Sphere>(vec3( 1.0,    0.0, -1.0),   0.5, right_material));
    return world;
}

auto book1(Sampler& sampler) -> HittableList
{
    HittableList world;

    auto ground_material = PrincipledBSDF::diffuse(vec3(0.5, 0.5, 0.5), 0.9);
    world.push(std::make_shared<Sphere>(vec3(0.0, -1000.0, 0.0), 1000.0, ground_material));

    for (i32 a = -11; a < 11; ++a) {
        for (i32 b = -11; b < 11; ++b) {
            real const choose_mat = sampler.sample_1d();
            vec3 const center(
                static_cast<real>(a) + 0.9 * sampler.sample_1d(),
                0.2,
                static_cast<real>(b) + 0.9 * sampler.sample_1d()
            );

            if (glm::length(center - vec3(4.0, 0.2, 0.0)) > 0.9) {
                std::shared_ptr<Material> sphere_material;

                if (choose_mat < 0.65) {
                    // Matte / subsurface / sheen fabric
                    vec3 const c0(sampler.sample_1d(), sampler.sample_1d(), sampler.sample_1d());
                    vec3 const c1(sampler.sample_1d(), sampler.sample_1d(), sampler.sample_1d());
                    sphere_material = std::make_shared<PrincipledBSDF>(PrincipledBSDF::Config {
                        .base_color = c0 * c1,
                        .roughness  = sampler.uniform(0.4, 1.0),
                        .subsurface = sampler.uniform(0.0, 0.4),
                        .specular   = 0.3,
                        .sheen      = sampler.uniform(0.0, 0.5)
                    });
                } else if (choose_mat < 0.82) {
                    // Glossy clearcoat plastic
                    vec3 const albedo(sampler.sample_1d(), sampler.sample_1d(), sampler.sample_1d());
                    sphere_material = PrincipledBSDF::plastic(albedo, sampler.uniform(0.05, 0.35), 1.0);
                } else if (choose_mat < 0.95) {
                    // Anisotropic / rough conductor
                    vec3 const albedo(
                        sampler.uniform(0.5, 1.0),
                        sampler.uniform(0.5, 1.0),
                        sampler.uniform(0.5, 1.0)
                    );
                    real const roughness = sampler.uniform(0.0, 0.5);
                    sphere_material = PrincipledBSDF::metal(albedo, roughness, sampler.uniform(0.0, 0.8));
                } else {
                    // Smooth or frosted glass
                    sphere_material = PrincipledBSDF::dielectric(1.5, sampler.uniform(0.0, 0.15));
                }

                world.push(std::make_shared<Sphere>(center, 0.2, sphere_material));
            }
        }
    }

    auto material1 = PrincipledBSDF::dielectric(1.5, 0.0);
    world.push(std::make_shared<Sphere>(vec3(0.0, 1.0, 0.0), 1.0, material1));

    auto material2 = std::make_shared<PrincipledBSDF>(PrincipledBSDF::Config {
        .base_color          = vec3(0.4, 0.2, 0.1),
        .roughness           = 0.35,
        .subsurface          = 0.25,
        .specular            = 0.5,
        .clearcoat           = 1.0,
        .clearcoat_roughness = 0.03
    });
    world.push(std::make_shared<Sphere>(vec3(-4.0, 1.0, 0.0), 1.0, material2));

    auto material3 = PrincipledBSDF::metal(vec3(0.7, 0.6, 0.5), 0.0);
    world.push(std::make_shared<Sphere>(vec3(4.0, 1.0, 0.0), 1.0, material3));

    return world;
}

auto main() -> int
{
    // usize const image_width = 400;
    // usize const image_height = 300;
    // usize const image_samples = 32;
    // usize const image_depth = 64;

    usize const image_width = 1920;
    usize const image_height = 1080;
    usize const image_samples = 1024;
    usize const image_depth = 128;

    std::string const output_filename = "image.png";

    Film film(image_width, image_height);

    // ThinLensCamera::Config camera_config {
    //     .width = image_width,
    //     .height = image_height,
    //     .vfov = 90.0,
    //     .lookfrom = { 0.0, 0.0, 0.0 },
    //     .lookat = { 0.0, 0.0, -1.0 },
    //     .vup = { 0.0, 1.0, 0.0 },
    //     .defocus_angle = 0.0,
    //     .focal_length = 1.0
    // };

    ThinLensCamera::Config camera_config {
        .width = image_width,
        .height = image_height,
        .vfov = 20.0,
        .lookfrom = { 13.0, 2.0, 3.0 },
        .lookat = { 0.0, 0.0, 0.0 },
        .vup = { 0.0, 1.0, 0.0 },
        .defocus_angle = 0.6,
        .focal_length = 10.0
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
    integrator.render(world, camera, film);
    auto const end_time = std::chrono::steady_clock::now();

    f64 const elapsed_sec = std::chrono::duration<f64>(end_time - start_time).count();
    f64 const msamples_per_sec = (static_cast<f64>(total_samples) / 1e6) / std::max(elapsed_sec, 1e-9);

    film.save(output_filename);
    // image.save(output_filename);

    // post-render diagnostics
    std::print("\n\033[1;32m=== Render Complete ===\033[0m\n");
    std::print("  Render Time    : {:.2f}s\n", elapsed_sec);
    std::print("  Throughput     : {:.2f} MSamples/s ({:.0f} pixels/s)\n", msamples_per_sec, static_cast<f64>(total_pixels) / std::max(elapsed_sec, 1e-9));
    std::print("  Saved Output   : {}\n", output_filename);
}
