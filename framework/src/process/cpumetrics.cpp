#include "process/cpumetrics.h"

#include <cstdint>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace process {

    // Helper function to actually read from procfs
    bool PollRawSample(pid_t pid, ProcessCpuSample& sample) {
        std::string line;

        sample.valid = false;
        sample.timestamp = std::chrono::steady_clock::now();

        // Getting CPU info from /proc/[pid]/stat
        std::ifstream stat_file("/proc/" + std::to_string(pid) + "/stat");
        if (!stat_file.is_open()) { return false; }

        std::getline(stat_file, line);

        size_t close_paren = line.rfind(')');
        if (close_paren == std::string::npos) { return false; }

        std::istringstream iss(line.substr(close_paren + 2));
        std::string token;

        int64_t utime = 0, stime = 0;
        for (int i = 3; i <= 15; i++) {
            if (!(iss >> token)) { return false; }
            if (i == 14) { utime = static_cast<int64_t>(std::stoll(token)); }
            if (i == 15) { stime = static_cast<int64_t>(std::stoll(token)); }
        }
        int64_t new_cpu_ticks = utime + stime;

        // Getting RAM info from /proc/[pid]/status
        std::ifstream status_file("/proc/" + std::to_string(pid) + "/status");
        if (!status_file.is_open()) { return false; }

        int64_t new_rss_kb = -1;
        while (std::getline(status_file, line)) {
            if (line.compare(0, 6, "VmRSS:") == 0) {
                std::istringstream lss(line.substr(6));
                int64_t rss = -1;
                lss >> rss;
                new_rss_kb = rss;
                break;
            }
        }
        if (new_rss_kb < 0) { return false; }

        sample.cpu_ticks = new_cpu_ticks;
        sample.rss_kb = new_rss_kb;
        sample.valid = true;
        return true;
    }

    bool PollProcessCpuMetrics(pid_t pid, ProcessCpuSample& prev, ProcessCpuMetrics& metrics) {
        metrics.valid = false;

        ProcessCpuSample current;
        if (!PollRawSample(pid, current)) {
            prev.valid = false;
            return false;
        }

        if (!prev.valid) {
            prev = current;
            return false;
        }

        static const int64_t ticks_per_sec = static_cast<int64_t>(sysconf(_SC_CLK_TCK));
        double elapsed_sec = std::chrono::duration<double>(current.timestamp - prev.timestamp).count();
        double cpu_secs_used = static_cast<double>(current.cpu_ticks - prev.cpu_ticks) / static_cast<double>(ticks_per_sec);

        metrics.cpu_percent = elapsed_sec > 0.0 ? (cpu_secs_used / elapsed_sec) * 100.0 : 0.0;
        metrics.ram_kb = current.rss_kb;
        metrics.valid = true;

        prev = current;
        return true;
    }

} // namespace process
