#include "sampler/independent_sampler.hpp"
#include "camera/thinlens_camera.hpp"
#include "integrator/path_integrator.hpp"

#include "hittable/hittable_list.hpp"
#include "hittable/sphere.hpp"

#include "acceleration/bvh.hpp"

#include "material/principled.hpp"

#include "sky/hosek_wilkie.hpp"

using namespace rtweekend;

auto principled_showcase(Sampler& sampler) -> HittableList;
auto book1(Sampler& sampler) -> HittableList;

auto main() -> int
{
    // usize const image_width = 400;
    // usize const image_height = 300;
    // usize const image_samples = 32768;
    // usize const image_depth = 64;

    usize const image_width = 1920;
    usize const image_height = 1080;
    usize const image_samples = 1024;
    usize const image_depth = 64;

    std::string const output_filename = "image.png";

    // auto film = std::make_shared<Film>(FilmConfig {
    //     .width = image_width,
    //     .height = image_height
    // });

    // auto camera = std::make_shared<ThinLensCamera>(ThinLensCameraConfig {
    //     .width = image_width,
    //     .height = image_height,
    //     .vfov = 20.0,
    //     .lookfrom = { 13.0, 2.0, 3.0 },
    //     .lookat = { 0.0, 0.0, 0.0 },
    //     .vup = { 0.0, 1.0, 0.0 },
    //     .defocus_angle = 0.6,
    //     .focal_length = 10.0
    // });

    // auto sampler = std::make_shared<IndependentSampler>();

    // HittableList world;
    // // world = book1(*sampler);
    // world = principled_showcase(*sampler);

    // BVH scene(world);

    // auto sky = std::make_shared<HosekWilkieSky>(HosekWilkieConfig {
    //     .sun_direction = { 0.45, 0.38, 0.80 },
    //     .turbidity = 2.5,
    //     .ground_albedo = { 0.3, 0.3, 0.3 },
    //     .sun_angular_radius_deg = 0.255,
    //     .radiance_scale = 0.065,
    //     .sun_intensity = 1.0,
    //     .enable_sun_disc = true,
    //     .enable_ground_bounce = true
    // });

    // auto integrator = std::make_shared<PathIntegrator>(sampler, image_samples, image_depth, sky);
    // integrator->render(scene, *camera, *film);

    // film->save(output_filename);

    auto film = std::make_shared<Film>(FilmConfig {
        .width       = image_width,
        .height      = image_height,
        .exposure_ev = 0.15
    });

    // Focused directly on the center amber-glass hero sphere at (0, 1, 0)
    auto camera = std::make_shared<ThinLensCamera>(ThinLensCameraConfig {
        .width         = image_width,
        .height        = image_height,
        .vfov          = 28.0,
        .lookfrom      = { 0.0, 2.05, 7.60 },
        .lookat        = { 0.0, 0.95, 0.00 },
        .vup           = { 0.0, 1.00, 0.00 },
        .defocus_angle = 0.35,
        .focal_length  = 6.75
    });

    auto sampler = std::make_shared<IndependentSampler>();

    HittableList const world = principled_showcase(*sampler);
    BVH const scene(world);

    // Low golden-hour sun casting warm grazing backlight across the gallery
    auto sky = std::make_shared<HosekWilkieSky>(HosekWilkieConfig {
        .sun_direction          = { -0.52, 0.17, -0.84 },
        .turbidity              = 3.2,
        .ground_albedo          = {  0.25, 0.22,  0.20 },
        .sun_angular_radius_deg = 0.32,
        .radiance_scale         = 0.055,
        .sun_intensity          = 1.15,
        .enable_sun_disc        = true,
        .enable_ground_bounce   = true
    });

    auto integrator = std::make_shared<PathIntegrator>(sampler, image_samples, image_depth, sky);
    integrator->render(scene, *camera, *film);

    film->save(output_filename);
}

auto principled_showcase(Sampler& sampler) -> HittableList
{
    HittableList world;

    // -------------------------------------------------------------------------
    // 1. Ground & Background Architectural Monoliths
    // -------------------------------------------------------------------------
    auto basalt_floor = std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
        .base_color          = vec3(0.07, 0.075, 0.085),
        .metallic            = 0.0,
        .roughness           = 0.28,
        .specular            = 0.65,
        .clearcoat           = 0.35,
        .clearcoat_roughness = 0.08
    });
    world.push(std::make_shared<Sphere>(vec3(0.0, -1000.0, 0.0), 1000.0, basalt_floor));

    auto charcoal_monolith = PrincipledBSDF::diffuse(vec3(0.05, 0.055, 0.065), 0.85, 0.15);
    // world.push(std::make_shared<Sphere>(vec3(-4.2, 3.2, -5.8), 3.2, charcoal_monolith));
    // world.push(std::make_shared<Sphere>(vec3( 4.2, 3.2, -5.8), 3.2, charcoal_monolith));

    // -------------------------------------------------------------------------
    // 2. Emissive Area Lights (Warm Key + Cool Rim + Overhead Fill)
    // -------------------------------------------------------------------------
    auto warm_key_light  = PrincipledBSDF::emissive(vec3(28.0, 16.0, 7.5));
    auto cool_fill_light = PrincipledBSDF::emissive(vec3(6.0, 14.0, 26.0));
    auto soft_top_light  = PrincipledBSDF::emissive(vec3(14.0, 13.5, 12.0));

    world.push(std::make_shared<Sphere>(vec3(-4.8, 3.4,  2.2), 0.65, warm_key_light));
    world.push(std::make_shared<Sphere>(vec3( 4.8, 2.8,  2.0), 0.55, cool_fill_light));
    world.push(std::make_shared<Sphere>(vec3( 0.0, 5.2, -0.5), 0.85, soft_top_light));

    // -------------------------------------------------------------------------
    // 3. Five Hero Spheres (Semicircle Arc)
    // -------------------------------------------------------------------------

    // Hero 1 (Center): Solid Amber Elixir Glass + Hollow Thin-Walled Bubble Core
    auto amber_glass = PrincipledBSDF::dielectric(
        1.53,
        0.015,
        vec3(0.96, 0.34, 0.07),
        0.55
    );
    auto inner_air_bubble = PrincipledBSDF::thin_dielectric(1.53, 0.0, vec3(1.0));
    world.push(std::make_shared<Sphere>(vec3(0.0, 1.0, 0.0), 1.0, amber_glass));
    world.push(std::make_shared<Sphere>(vec3(0.0, 1.0, 0.0), 0.42, inner_air_bubble));

    // Hero 2 (Inner Left): Anisotropic Gold Base under Tinted Crimson Clearcoat
    auto candy_lacquer = std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
        .base_color          = vec3(0.95, 0.72, 0.28),
        .metallic            = 0.95,
        .roughness           = 0.28,
        .anisotropic         = 0.80,
        .clearcoat           = 1.0,
        .clearcoat_roughness = 0.015,
        .clearcoat_ior       = 1.55,
        .clearcoat_tint      = vec3(0.82, 0.12, 0.10)
    });
    world.push(std::make_shared<Sphere>(vec3(-2.15, 1.0, -0.35), 1.0, candy_lacquer));

    // Hero 3 (Inner Right): Imperial Jade Subsurface + Ceramic Clearcoat Glaze
    auto imperial_jade = std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
        .base_color          = vec3(0.14, 0.65, 0.42),
        .metallic            = 0.0,
        .roughness           = 0.32,
        .subsurface          = 0.85,
        .specular            = 0.55,
        .clearcoat           = 1.0,
        .clearcoat_roughness = 0.03,
        .clearcoat_ior       = 1.52,
        .clearcoat_tint      = vec3(0.92, 0.99, 0.95)
    });
    world.push(std::make_shared<Sphere>(vec3(2.15, 1.0, -0.35), 1.0, imperial_jade));

    // Hero 4 (Outer Left): Royal Indigo Velvet (Charlie Microfiber Sheen)
    auto royal_velvet = std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
        .base_color      = vec3(0.13, 0.06, 0.30),
        .metallic        = 0.0,
        .roughness       = 0.88,
        .subsurface      = 0.15,
        .specular        = 0.10,
        .sheen           = 1.0,
        .sheen_tint      = 0.25,
        .sheen_roughness = 0.26
    });
    world.push(std::make_shared<Sphere>(vec3(-4.25, 1.0, -1.20), 1.0, royal_velvet));

    // Hero 5 (Outer Right): Anisotropic Brushed Rose-Gold Conductor
    auto brushed_rose_gold = std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
        .base_color    = vec3(0.94, 0.62, 0.50),
        .metallic      = 1.0,
        .roughness     = 0.20,
        .specular_tint = 0.85,
        .anisotropic   = 0.88
    });
    world.push(std::make_shared<Sphere>(vec3(4.25, 1.0, -1.20), 1.0, brushed_rose_gold));

    // -------------------------------------------------------------------------
    // 4. Foreground Curated Swatch Arc (7 Spheres, r = 0.32)
    // -------------------------------------------------------------------------
    std::shared_ptr<PrincipledBSDF> const swatches[7] = {
        // 0: Thin-walled soap bubble
        PrincipledBSDF::thin_dielectric(1.42, 0.005, vec3(0.94, 0.97, 1.0)),
        // 1: Terracotta clay with subsurface & warm sheen
        std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
            .base_color      = vec3(0.76, 0.30, 0.16),
            .roughness       = 0.72,
            .subsurface      = 0.45,
            .specular        = 0.25,
            .sheen           = 0.45,
            .sheen_tint      = 0.60,
            .sheen_roughness = 0.40
        }),
        // 2: Frosted cobalt glass
        PrincipledBSDF::dielectric(1.52, 0.12, vec3(0.14, 0.38, 0.95), 0.35),
        // 3: Automotive teal clearcoat plastic
        PrincipledBSDF::plastic(vec3(0.05, 0.48, 0.58), 0.12, 1.0),
        // 4: Frosted emerald glass
        PrincipledBSDF::dielectric(1.56, 0.08, vec3(0.12, 0.85, 0.36), 0.35),
        // 5: Brushed anisotropic platinum
        PrincipledBSDF::metal(vec3(0.86, 0.88, 0.92), 0.14, 0.75),
        // 6: Mirror gold sphere
        PrincipledBSDF::metal(vec3(0.98, 0.78, 0.32), 0.01, 0.0)
    };

    for (i32 i = 0; i < 7; ++i) {
        real const t = (static_cast<real>(i) - 3.0) / 3.0; // [-1, 1]
        real const x = t * 3.1;
        real const z = 1.95 - 0.45 * t * t;
        world.push(std::make_shared<Sphere>(vec3(x, 0.32, z), 0.32, swatches[i]));
    }

    // -------------------------------------------------------------------------
    // 5. Scattered Micro-Orbs (Jewels, Pearls, and Glowing Embers)
    // -------------------------------------------------------------------------
    for (i32 i = 0; i < 36; ++i) {
        real const angle  = sampler.uniform(-1.15, 1.15);
        real const radius = sampler.uniform(2.55, 4.35);
        real const r_orb  = sampler.uniform(0.08, 0.14);

        vec3 const center(
            radius * std::sin(angle),
            r_orb,
            0.85 + radius * std::cos(angle) * 0.42
        );

        // Keep clear of the 7 foreground swatch spheres
        bool overlaps = false;
        for (i32 j = 0; j < 7; ++j) {
            real const t = (static_cast<real>(j) - 3.0) / 3.0;
            vec3 const swatch_p(t * 3.1, 0.32, 1.95 - 0.45 * t * t);
            if (glm::length(center - swatch_p) < (0.32 + r_orb + 0.08)) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) continue;

        real const pick = sampler.sample_1d();
        std::shared_ptr<PrincipledBSDF> mat;
        if (pick < 0.35) {
            // Pearlescent clearcoat bead
            vec3 const tint(
                sampler.uniform(0.65, 0.98),
                sampler.uniform(0.60, 0.95),
                sampler.uniform(0.65, 0.98)
            );
            mat = PrincipledBSDF::plastic(tint, sampler.uniform(0.08, 0.25), 1.0);
        } else if (pick < 0.70) {
            // Colored glass marble
            vec3 const glass_col(
                sampler.uniform(0.25, 0.98),
                sampler.uniform(0.25, 0.98),
                sampler.uniform(0.25, 0.98)
            );
            mat = PrincipledBSDF::dielectric(1.52, sampler.uniform(0.0, 0.06), glass_col, 0.18);
        } else if (pick < 0.92) {
            // Polished metallic bearing
            vec3 const metal_col(
                sampler.uniform(0.65, 0.98),
                sampler.uniform(0.55, 0.92),
                sampler.uniform(0.45, 0.90)
            );
            mat = PrincipledBSDF::metal(metal_col, sampler.uniform(0.02, 0.18), sampler.uniform(0.0, 0.7));
        } else {
            // Warm glowing ember
            mat = PrincipledBSDF::emissive(vec3(18.0, 6.5, 1.8));
        }

        world.push(std::make_shared<Sphere>(center, r_orb, mat));
    }

    return world;
}

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
                std::shared_ptr<PrincipledBSDF> sphere_material;

                if (choose_mat < 0.65) {
                    // Matte / subsurface / sheen fabric
                    vec3 const c0(sampler.sample_1d(), sampler.sample_1d(), sampler.sample_1d());
                    vec3 const c1(sampler.sample_1d(), sampler.sample_1d(), sampler.sample_1d());
                    sphere_material = std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
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

    auto material2 = std::make_shared<PrincipledBSDF>(PrincipledBSDFConfig {
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
