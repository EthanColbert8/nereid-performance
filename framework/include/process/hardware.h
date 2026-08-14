#pragma once

#include "logging/logger.h"

#include <atomic>
#include <chrono>
#include <vector>
#include <sys/types.h>

#include <nlohmann/json.hpp>

namespace process {

    struct HardwareMetrics {
        // std::vector<double> gpu_util;
        // std::vector<double> gpu_mem;
        std::vector<double> cpu_util;
        std::vector<double> ram_mb;
        std::vector<double> elapsed_sec;
        std::chrono::steady_clock::time_point start;
        std::chrono::system_clock::time_point wall_start;
        int count;
        // bool gpu_metrics_available;
    };

    struct HardwareMetricsContext {
        std::chrono::milliseconds interval;
        pid_t server_pid;
    };

    void ScrapeHardwareMetrics(std::atomic_bool& stop, const HardwareMetricsContext& context, HardwareMetrics& metrics);

    nlohmann::json GenerateHardwareMetricsReport(const HardwareMetrics& metrics, logging::Logger& logger);

} // namespace process
