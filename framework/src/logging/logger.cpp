#include "logging/logger.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <chrono>
#include <ctime>
#include <vector>

namespace logging {

    static const char* LevelToString(LogLevel level) {
        switch (level) {
            case DEBUG: return "DEBUG";
            case INFO: return "INFO";
            case WARNING: return "WARNING";
            case ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    size_t FormatTimestamp(char* out, size_t out_size) {
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

    Logger::Logger(FILE* out, LogLevel level, size_t capacity) : min_level(level), buffer(capacity), out(out) {}

    Logger::~Logger() {
        flush();

        if (out != stdout && out != stderr) {
            fclose(out);
        }
    }

    void Logger::flush() {
        if (pos > 0) {
            fwrite(buffer.data(), 1, pos, out);
            pos = 0;
        }
    }

    void Logger::write(const char* data, size_t len) {
        if (len > buffer.size()) {
            flush();
            fwrite(data, 1, len, out);
            return;
        }
        if (len > buffer.size() - pos) { flush(); }

        std::memcpy(buffer.data() + pos, data, len);
        pos += len;
    }

    void Logger::write_log(LogLevel level, const char* format, va_list args) {
        if (level < min_level) { return; }

        char line[512];
        line[0] = '[';
        line[1] = '\0';
        size_t pos = 1;

        pos += FormatTimestamp(line + pos, sizeof(line) - pos);

        pos += snprintf(line + pos, sizeof(line) - pos, " - %s]: ", LevelToString(level));

        int n = vsnprintf(line + pos, sizeof(line) - pos, format, args);
        if (n < 0) { return; }
        pos += static_cast<size_t>(n);

        if (pos >= sizeof(line)) { pos = sizeof(line) - 1; }

        write(line, pos);
        write("\n", 1);
    }

    void Logger::log(LogLevel level, const char* format, ...) {
        va_list args;
        va_start(args, format);
        write_log(level, format, args);
        va_end(args);
    }

    void Logger::debug(const char* format, ...) {
        va_list args;
        va_start(args, format);
        write_log(DEBUG, format, args);
        va_end(args);
    }

    void Logger::info(const char* format, ...) {
        va_list args;
        va_start(args, format);
        write_log(INFO, format, args);
        va_end(args);
    }

    void Logger::warning(const char* format, ...) {
        va_list args;
        va_start(args, format);
        write_log(WARNING, format, args);
        va_end(args);
    }

    void Logger::error(const char* format, ...) {
        va_list args;
        va_start(args, format);
        write_log(ERROR, format, args);
        va_end(args);
    }

} // namespace logging
