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

    auto SamplerIntegrator::render(const Hittable& world, const Camera& camera, Image& image) const -> void
    {
        usize const width = image.width();
        usize const height = image.height();

        ProgressBar progress(height);
        std::atomic<usize> next_row { 0 };

        usize const num_threads = std::max<usize>(1, std::thread::hardware_concurrency() / 2);

        std::vector<std::jthread> workers;
        workers.reserve(num_threads);

        for (usize t = 0; t < num_threads; ++t) {
            workers.emplace_back([&, this]() {
                auto sampler = m_sampler->clone();

                while (true) {
                    usize const j = next_row.fetch_add(1, std::memory_order_relaxed);
                    if (j >= height) break;

                    for (usize i = 0; i < width; ++i) {
                        glm::vec3 radiance(0.0f);

                        for (usize s = 0; s < m_samples; ++s) {
                            sampler->seed_pixel(i, j, s);

                            CameraSample sample {
                                .offset = sampler->sample_2d(),
                                .uv = sampler->sample_2d(),
                                .time = sampler->sample_1d()
                            };

                            Ray const ray = camera.genray(i, j, sample);
                            radiance += li(ray, world, *sampler);
                        }

                        radiance /= static_cast<f32>(m_samples);
                        image.set(i, j, radiance);
                    }

                    progress.update();
                }
            });
        }

        workers.clear();
        progress.finish();
    }

}
