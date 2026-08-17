#include "logging/logger.h"
#include "utils/timestamp.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <vector>

namespace logging {

    static const char* LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
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

        pos += utils::FormatTimestamp(line + pos, sizeof(line) - pos);

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
        write_log(LogLevel::DEBUG, format, args);
        va_end(args);
    }

    void Logger::info(const char* format, ...) {
        va_list args;
        va_start(args, format);
        write_log(LogLevel::INFO, format, args);
        va_end(args);
    }

    void Logger::warning(const char* format, ...) {
        va_list args;
        va_start(args, format);
        write_log(LogLevel::WARNING, format, args);
        va_end(args);
    }

    void Logger::error(const char* format, ...) {
        va_list args;
        va_start(args, format);
        write_log(LogLevel::ERROR, format, args);
        va_end(args);
    }

} // namespace logging
