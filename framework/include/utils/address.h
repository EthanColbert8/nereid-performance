#pragma once

#include "utils/errors.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace utils {

    inline bool BuildAddress(const char* address, const char* port, std::string* combined_address, std::string* error_message) {
        if (address == nullptr || *address == '\0') {
            SetError(error_message, "server address is empty");
            return false;
        }

        if (port == nullptr || *port == '\0') {
            SetError(error_message, "server port is empty");
            return false;
        }

        errno = 0;
        char* end = nullptr;
        const long parsed_port = std::strtol(port, &end, 10);
        if (errno != 0 || end == port || *end != '\0' || parsed_port <= 0 || parsed_port > 65535) {
            SetError(error_message, std::string("invalid server port: ") + port);
            return false;
        }

        const int required_size = std::snprintf(nullptr, 0, "%s:%ld", address, parsed_port);
        if (required_size < 0) {
            SetError(error_message, "failed to format server address");
            return false;
        }

        combined_address->resize(static_cast<size_t>(required_size));
        std::snprintf(&(*combined_address)[0], static_cast<size_t>(required_size) + 1, "%s:%ld", address, parsed_port);
        return true;
    }

}
