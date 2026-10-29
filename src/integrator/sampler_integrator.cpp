#include "sampler_integrator.hpp"

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include "progress.hpp"

namespace rtweekend {

    SamplerIntegrator::SamplerIntegrator(std::unique_ptr<Sampler> sampler, usize sample)
        : m_sampler(std::move(sampler))
        , m_samples(sample)
    {
    }

    auto SamplerIntegrator::render(const Hittable& world, const Camera& camera, Film& film) const -> void
    {
        usize const width = film.width();
        usize const height = film.height();

        constexpr usize tile_size = 16;
        usize const tiles_x = (width  + tile_size - 1) / tile_size;
        usize const tiles_y = (height + tile_size - 1) / tile_size;
        usize const total_tiles = tiles_x * tiles_y;

        ProgressBar progress(total_tiles);
        std::atomic<usize> next_tile { 0 };

        usize const num_threads = std::max<usize>(1, std::thread::hardware_concurrency() / 2);

        std::vector<std::jthread> workers;
        workers.reserve(num_threads);

        for (usize t = 0; t < num_threads; ++t) {
            workers.emplace_back([&, this]() {
                auto sampler = m_sampler->clone();
                while (true) {
                    usize const tile_idx = next_tile.fetch_add(1, std::memory_order_relaxed);
                    if (tile_idx >= total_tiles) break;

                    usize const tx = tile_idx % tiles_x;
                    usize const ty = tile_idx / tiles_x;
                    usize const x0 = tx * tile_size;
                    usize const y0 = ty * tile_size;
                    usize const x1 = std::min(x0 + tile_size, width);
                    usize const y1 = std::min(y0 + tile_size, height);

                    FilmTile tile = film.create_tile(x0, y0, x1, y1);

                    for (usize j = y0; j < y1; ++j) {
                        for (usize i = x0; i < x1; ++i) {
                            for (usize s = 0; s < m_samples; ++s) {
                                sampler->seed_pixel(i, j, s);

                                CameraSample sample {
                                    .offset = sampler->sample_2d(),
                                    .uv = sampler->sample_2d(),
                                    .time = sampler->sample_1d()
                                };

                                Ray const ray = camera.genray(i, j, sample);
                                vec3 const radiance = li(ray, world, *sampler);

                                vec2 const p_film {
                                    static_cast<real>(i) + sample.offset.x,
                                    static_cast<real>(j) + sample.offset.y
                                };

                                tile.add_sample(p_film, radiance);
                            }
                        }
                    }

                    film.merge_tile(tile);
                    progress.update();
                }
            });
        }

        workers.clear();
        progress.finish();
    }

}
