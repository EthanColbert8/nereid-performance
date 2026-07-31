#pragma once

namespace utils {

    void SetError(std::string* error_message, const char* message) {
        if (error_message != nullptr) {
            *error_message = message;
        }
    }
    void SetError(std::string* error_message, const std::string& message) {
        if (error_message != nullptr) {
            *error_message = message;
        }
    }

}
