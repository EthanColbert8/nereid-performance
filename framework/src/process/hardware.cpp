#include "process/hardware.h"
#include "process/cpumetrics.h"
#include "logging/logger.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <vector>

#include <nlohmann/json.hpp>

namespace process {

    void ScrapeHardwareMetrics(std::atomic_bool& stop, const HardwareMetricsContext& context, HardwareMetrics& metrics) {
        // metrics.gpu_util.reserve(1000);
        // metrics.gpu_mem.reserve(1000);
        metrics.cpu_util.reserve(1000);
        metrics.ram_mb.reserve(1000);
        metrics.elapsed_sec.reserve(1000);
        metrics.count = 0;

        ProcessCpuSample prev_sample{};
        ProcessCpuMetrics cpu_metrics{};

        // Just gets us valid metrics on first loop iteration, can be skipped
        PollProcessCpuMetrics(context.server_pid, prev_sample, cpu_metrics);

        metrics.start = std::chrono::steady_clock::now();
        metrics.wall_start = std::chrono::system_clock::now();

        auto next_tick = std::chrono::steady_clock::now();
        while (!stop.load()) {
            next_tick += context.interval;

            // First poll is valid since earlier call set prev_sample to a valid state
            if (PollProcessCpuMetrics(context.server_pid, prev_sample, cpu_metrics)) {
                metrics.cpu_util.push_back(cpu_metrics.cpu_percent);
                metrics.ram_mb.push_back(static_cast<double>(cpu_metrics.ram_kb) / 1024.0); // Store RAM usage in MB
                metrics.elapsed_sec.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now() - metrics.start).count());
                metrics.count++;
            }

            std::this_thread::sleep_until(next_tick);
        }
    }

    nlohmann::json GenerateHardwareMetricsReport(const HardwareMetrics& metrics, logging::Logger& logger) {
        time_t start_time_t = std::chrono::system_clock::to_time_t(metrics.wall_start);

        auto start_ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(metrics.wall_start.time_since_epoch()).count();
        auto start_sec_fraction = (start_ms_since_epoch % 1000) / 100; // tenths of a second

        struct tm local_tm;
        localtime_r(&start_time_t, &local_tm);

        char start_time_str[32];
        size_t n = strftime(start_time_str, sizeof(start_time_str), "%Y-%m-%d %H:%M:%S", &local_tm);
        n += snprintf(start_time_str + n, sizeof(start_time_str) - n, ".%1lld", start_sec_fraction);
        
        if (n >= sizeof(start_time_str)) {
            logger.warning("Start time string truncated somehow");
        }

        bool cpu_util_ok = metrics.cpu_util.size() == metrics.count;
        bool ram_mb_ok = metrics.ram_mb.size() == metrics.count;
        bool elapsed_sec_ok = metrics.elapsed_sec.size() == metrics.count;
        if (!(cpu_util_ok && ram_mb_ok && elapsed_sec_ok)) {
            logger.error("Metrics data corrupted! Size mismatch: cpu_util=%zu, ram_mb=%zu, elapsed_sec=%zu, recorded count=%d",
                         metrics.cpu_util.size(), metrics.ram_mb.size(), metrics.elapsed_sec.size(), metrics.count);
        }

        nlohmann::json report = nlohmann::json::object();
        report["cpu_util"] = metrics.cpu_util;
        report["ram_mb"] = metrics.ram_mb;
        report["elapsed_sec"] = metrics.elapsed_sec;
        report["start_time"] = std::string(start_time_str);
        return report;
    }

} // namespace process
