#include "process/hardware.h"
#include "process/cpumetrics.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

namespace process {

    HardwareMetrics ScrapeHardwareMetrics(std::atomic_bool& stop, const HardwareMetricsContext& context, HardwareMetrics& metrics) {
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

        return metrics;
    }

} // namespace process
