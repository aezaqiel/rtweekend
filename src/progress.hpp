#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <format>
#include <mutex>
#include <print>
#include <string>
#include <string_view>

namespace rtweekend {

    class ProgressBar
    {
    public:
        explicit ProgressBar(usize total, std::string_view label = "rendering", usize bar_width = 32) noexcept
            : m_total(std::max<usize>(1, total))
            , m_bar_width(bar_width)
            , m_label(label)
            , m_start_time(Clock::now())
        {
            std::print(stderr, "\033[?25l");
            draw(0, Duration::zero());
        }

        ~ProgressBar() noexcept
        {
            finish();
        }

        ProgressBar(const ProgressBar&) = delete;
        auto operator=(const ProgressBar&) -> ProgressBar& = delete;

        auto update(usize step = 1) noexcept -> void
        {
            usize const completed = m_completed.fetch_add(step, std::memory_order_relaxed) + step;
            auto const now = Clock::now();

            // Always draw the final 100% update
            if (completed >= m_total) {
                std::lock_guard lock(m_mutex);
                if (!m_finished) {
                    draw(m_total, now - m_start_time);
                }
                return;
            }

            // Fast lock-free throttle check (~20 FPS max redraw rate)
            auto const last_ns = m_last_draw_ns.load(std::memory_order_relaxed);
            auto const now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_start_time).count();
            if (now_ns - last_ns < s_min_redraw_interval_ns) {
                return;
            }

            // Non-blocking lock: if another thread is already printing, keep tracing rays!
            if (m_mutex.try_lock()) {
                std::lock_guard lock(m_mutex, std::adopt_lock);
                if (!m_finished && (now_ns - m_last_draw_ns.load(std::memory_order_relaxed) >= s_min_redraw_interval_ns)) {
                    m_last_draw_ns.store(now_ns, std::memory_order_relaxed);
                    draw(std::min(completed, m_total), now - m_start_time);
                }
            }
        }

        auto finish() noexcept -> void
        {
            std::lock_guard lock(m_mutex);
            if (m_finished) return;
            m_finished = true;

            draw(m_total, Clock::now() - m_start_time);
            // Restore terminal cursor and move to a new line
            std::print(stderr, "\033[?25h\n");
            std::fflush(stderr);
        }

    private:
        using Clock = std::chrono::steady_clock;
        using Duration = Clock::duration;

        static constexpr i64 s_min_redraw_interval_ns { 50'000'000 }; // 50 ms (20 Hz)

        usize m_total { 1 };
        usize m_bar_width { 32 };
        std::string m_label;
        Clock::time_point m_start_time;

        std::atomic<usize> m_completed { 0 };
        std::atomic<i64> m_last_draw_ns { -s_min_redraw_interval_ns };
        std::mutex m_mutex;
        bool m_finished { false };

        static auto format_duration(f64 seconds) -> std::string
        {
            if (seconds < 0.0 || !std::isfinite(seconds)) {
                return "--:--";
            }
            u64 const total_secs = static_cast<u64>(seconds + 0.5);
            u64 const hrs = total_secs / 3600;
            u64 const mins = (total_secs % 3600) / 60;
            u64 const secs = total_secs % 60;

            if (hrs > 0) {
                return std::format("{:02}:{:02}:{:02}", hrs, mins, secs);
            }
            return std::format("{:02}:{:02}", mins, secs);
        }

        auto draw(usize completed, Duration elapsed_dur) const noexcept -> void
        {
            static constexpr std::string_view s_blocks[8] = {
                " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉"
            };

            f64 const fraction = std::clamp(static_cast<f64>(completed) / static_cast<f64>(m_total), 0.0, 1.0);
            f64 const exact_fill = fraction * static_cast<f64>(m_bar_width);
            usize const full_blocks = static_cast<usize>(exact_fill);
            usize const part_index = static_cast<usize>((exact_fill - static_cast<f64>(full_blocks)) * 8.0);

            std::string bar;
            bar.reserve(m_bar_width * 3); // UTF-8 block chars are 3 bytes each

            for (usize i = 0; i < full_blocks; ++i) {
                bar += "█";
            }
            if (full_blocks < m_bar_width) {
                bar += s_blocks[part_index];
                for (usize i = full_blocks + 1; i < m_bar_width; ++i) {
                    bar += ' ';
                }
            }

            f64 const elapsed_sec = std::chrono::duration<f64>(elapsed_dur).count();
            f64 const eta_sec = (completed > 0 && completed < m_total)
                ? (elapsed_sec * static_cast<f64>(m_total - completed) / static_cast<f64>(completed))
                : 0.0;

            // \r returns to start of line, \033[K clears any leftover trailing characters
            std::print(
                stderr,
                "\r\033[K\033[1;36m{}\033[0m [\033[1;32m{}\033[0m] {:6.2f}% ({}/{}) | Elapsed: {} | ETA: {}",
                m_label,
                bar,
                fraction * 100.0,
                completed,
                m_total,
                format_duration(elapsed_sec),
                completed >= m_total ? "00:00" : format_duration(eta_sec)
            );
            std::fflush(stderr);
        }
    };

}
