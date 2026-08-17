#pragma once

#include "logging/logger.h"

#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace nereid {

    bool WaitForServerReady(const char* server_address, int server_port, pid_t server_pid, int startup_timeout_secs, logging::Logger& logger);
    bool WaitForServerReady(const char* server_address, int server_port, int startup_timeout_secs, logging::Logger& logger);

} // namespace nereid
