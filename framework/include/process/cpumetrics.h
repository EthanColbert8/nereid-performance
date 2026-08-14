#pragma once

#include <cstdint>
#include <chrono>
#include <sys/types.h>

namespace process {

    struct ProcessCpuSample {
        std::chrono::steady_clock::time_point timestamp;
        int64_t cpu_ticks;
        int64_t rss_kb;
        bool valid;
    };

    struct ProcessCpuMetrics {
        double cpu_percent;
        int64_t ram_kb;
        bool valid;
    };

    bool PollProcessCpuMetrics(pid_t pid, ProcessCpuSample& prev, ProcessCpuMetrics& metrics);

} // namespace process

