#pragma once

#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace nereid {

    bool WaitForServerReady(const char* server_address, int server_port, pid_t server_pid, int startup_timeout_secs, std::string* error_message);
    bool WaitForServerReady(const char* server_address, int server_port, int startup_timeout_secs, std::string* error_message);

} // namespace nereid
