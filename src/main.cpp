#include "sampler/independent_sampler.hpp"
#include "camera/thinlens_camera.hpp"
#include "integrator/path_integrator.hpp"

#include "hittable/hittable_list.hpp"
#include "hittable/bvh.hpp"
#include "hittable/sphere.hpp"

#include "material/principled.hpp"

#include "sky/hosek_wilkie.hpp"

using namespace rtweekend;

auto book1(Sampler& sampler) -> HittableList
{
    HittableList world;

    // auto light_material  = PrincipledBSDF::emissive(vec3(15.0, 12.0, 8.0));
    // world.push(std::make_shared<Sphere>(vec3(0.0, 2.2, -1.0), 0.5, light_material));

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
    usize const image_width = 400;
    usize const image_height = 300;
    usize const image_samples = 32;
    usize const image_depth = 64;

    // usize const image_width = 1920;
    // usize const image_height = 1080;
    // usize const image_samples = 1024;
    // usize const image_depth = 128;

    std::string const output_filename = "image.png";

    auto film = std::make_shared<Film>(FilmConfig {
        .width = image_width,
        .height = image_height
    });

    auto camera = std::make_shared<ThinLensCamera>(ThinLensCameraConfig {
        .width = image_width,
        .height = image_height,
        .vfov = 20.0,
        .lookfrom = { 13.0, 2.0, 3.0 },
        .lookat = { 0.0, 0.0, 0.0 },
        .vup = { 0.0, 1.0, 0.0 },
        .defocus_angle = 0.6,
        .focal_length = 10.0
    });

    auto sampler = std::make_shared<IndependentSampler>();

    HittableList world;
    world = book1(*sampler);

    BVH scene(world);

    auto sky = std::make_shared<HosekWilkieSky>(HosekWilkieConfig {
        .sun_direction = { 0.45, 0.38, 0.80 },
        .turbidity = 2.5,
        .ground_albedo = { 0.3, 0.3, 0.3 },
        .sun_angular_radius_deg = 0.255,
        .radiance_scale = 0.065,
        .sun_intensity = 1.0,
        .enable_sun_disc = true,
        .enable_ground_bounce = true
    });

    auto integrator = std::make_shared<PathIntegrator>(sampler, image_samples, image_depth, sky);
    integrator->render(scene, *camera, *film);

    film->save(output_filename);
}
