#pragma once

#include "logging/logger.h"

#include <string>
#include <sys/types.h>

namespace process {

    struct ServerProcess {
        pid_t pid;
        bool owned;
        bool running;
    };

    bool LaunchServer(const char* binary_path, ServerProcess& server, const char* log_path, logging::Logger& logger);
    void StopServer(ServerProcess& server, logging::Logger& logger);

} // namespace process
