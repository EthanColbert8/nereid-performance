#pragma once

#include "utils/errors.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace utils {

    inline bool BuildAddress(const char* address, int port, std::string* combined_address, std::string* error_message) {
        if (address == nullptr || *address == '\0') {
            SetError(error_message, "server address is empty");
            return false;
        }

        if (port <= 0 || port > 65535) {
            SetError(error_message, "server port must be a positive integer between 1 and 65535");
            return false;
        }

        const int required_size = std::snprintf(nullptr, 0, "%s:%ld", address, port);
        if (required_size < 0) {
            SetError(error_message, "failed to format server address");
            return false;
        }

        combined_address->resize(static_cast<size_t>(required_size));
        std::snprintf(&(*combined_address)[0], static_cast<size_t>(required_size) + 1, "%s:%ld", address, port);
        return true;
    }

}
