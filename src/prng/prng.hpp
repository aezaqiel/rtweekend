#pragma once

#include <random>

#include <glm/glm.hpp>

namespace rtweekend {

    namespace {

        template <typename T>
        struct IsGLMVec : std::false_type {};

        template <glm::length_t L, typename T, glm::qualifier Q>
        struct IsGLMVec<glm::vec<L, T, Q>> : std::true_type {};

        template <typename T>
        concept GLMVec = IsGLMVec<std::remove_cvref_t<T>>::value;

        template <typename T>
        concept Samplable = std::floating_point<T> || std::integral<T> || GLMVec<T>;
    
    }

    class PRNG
    {
    public:
        template <Samplable T>
        inline static auto sample() -> T
        {
            return sample_impl<T>(engine());
        }

        template <typename T>
        auto sample(T min, T max) -> T
        {
            return sample_impl<T>(engine(), min, max);
        }

    private:
        using EngineType = std::mt19937;

        inline static auto engine() noexcept -> EngineType&
        {
            thread_local EngineType s_engine { std::random_device{}() };
            return s_engine;
        }

        template <typename T>
        inline static auto sample_impl(EngineType& rng) noexcept -> T
        {
            if constexpr (std::same_as<T, f32>) {
                return static_cast<f32>(static_cast<u32>(rng()) >> 8) * 0x1.0p-24f;
            } else if constexpr (std::floating_point<T>) {
                u64 const hi = static_cast<u32>(rng());
                u64 const lo = static_cast<u32>(rng());
                u64 const bits = ((hi << 32) | lo) >> 11;
                return static_cast<T>(static_cast<f64>(bits) * 0x1.0p-53);
            } else if constexpr (std::integral<T>) {
                return static_cast<T>(rng());
            } else if constexpr (GLMVec<T>) {
                using ValueType = typename T::value_type;
                return [&]<usize... Is>(std::index_sequence<Is...>) {
                    return T{ (static_cast<void>(Is), sample_impl<ValueType>(rng))... };
                }(std::make_index_sequence<static_cast<usize>(T::length())>{});
            } else {
                static_assert(false, "unsupported type for PRNG::sample()");
            }
        }

        template <typename T>
        inline static auto sample_impl(EngineType& rng, T min, T max) noexcept -> T
        {
            if constexpr (std::floating_point<T>) {
                return min + sample_impl<T>(rng) * (max - min);
            } else if constexpr (std::integral<T>) {
                std::uniform_int_distribution<T> dist(min, max);
                return dist(rng);
            } else if constexpr (GLMVec<T>) {
                using ValueType = typename T::value_type;
                return [&]<usize... Is>(std::index_sequence<Is...>) {
                    return T{ sample_impl<ValueType>(rng, min[Is], max[Is])... };
                }(std::make_index_sequence<static_cast<usize>(T::length())>{});
            } else {
                static_assert(false, "unsupported type for PRNG::sample()");
            }
        }
    };

}
