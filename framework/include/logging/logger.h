#pragma once

#include <cstdio>
#include <vector>

namespace logging {

    enum class LogLevel {
        DEBUG = 0,
        INFO = 4,
        WARNING = 8,
        ERROR = 12,
    };

    static const char* LevelToString(LogLevel level);

    class Logger {
        private:
            LogLevel min_level;
            std::vector<char> buffer;
            size_t pos = 0;
            FILE* out;

            void write(const char* data, size_t len);
            void write_log(LogLevel level, const char* format, va_list args);

        public:
            explicit Logger(FILE* out, LogLevel level = LogLevel::INFO, size_t capacity = 16384);
            ~Logger();

            // Don't allow copying or assigning a logger instance
            Logger(const Logger&) = delete;
            Logger& operator=(const Logger&) = delete;

            void log(LogLevel level, const char* format, ...);
            void debug(const char* format, ...);
            void info(const char* format, ...);
            void warning(const char* format, ...);
            void error(const char* format, ...);

            void flush();
    };

} // namespace logging
