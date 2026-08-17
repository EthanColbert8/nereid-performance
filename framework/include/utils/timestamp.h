#pragma once

#include <cstdio>
#include <cstring>
#include <chrono>
#include <ctime>

namespace utils {

    inline size_t FormatTimestamp(char* out, size_t out_size) {
        auto now = std::chrono::system_clock::now();
        time_t now_time_t = std::chrono::system_clock::to_time_t(now);

        auto ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        auto sec_fraction = (ms_since_epoch % 1000) / 100; // tenths of a second

        struct tm local_tm;
        localtime_r(&now_time_t, &local_tm);

        size_t n = strftime(out, out_size, "%Y-%m-%d %H:%M:%S", &local_tm);
        n += snprintf(out + n, out_size - n, ".%1lld", sec_fraction);
        return n;
    }

} // namespace utils
