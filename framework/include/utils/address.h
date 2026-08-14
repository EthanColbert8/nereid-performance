#pragma once

#include <cstdio>
// #include <cstdlib>
#include <string>

namespace utils {

    inline bool BuildAddress(const char* address, int port, std::string* combined_address) {
        if (address == nullptr || *address == '\0') {
            return false;
        }

        if (port <= 0 || port > 65535) {
            return false;
        }

        const int required_size = std::snprintf(nullptr, 0, "%s:%ld", address, port);
        if (required_size < 0) {
            return false;
        }

        combined_address->resize(static_cast<size_t>(required_size));
        std::snprintf(&(*combined_address)[0], static_cast<size_t>(required_size) + 1, "%s:%ld", address, port);
        return true;
    }

}
