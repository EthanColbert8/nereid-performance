#pragma once

#include <string>

namespace utils {

    inline void SetError(std::string* error_message, const char* message) {
        if (error_message != nullptr) {
            *error_message = message;
        }
    }
    inline void SetError(std::string* error_message, const std::string& message) {
        if (error_message != nullptr) {
            *error_message = message;
        }
    }

}
