#pragma once

#include <string>
#include <sys/types.h>

namespace process {

    struct ServerProcess {
        pid_t pid;
        bool owned;
        bool running;
    };

    bool LaunchServer(const char* binary_path, ServerProcess* server, std::string* error_message);
    void StopServer(ServerProcess& server);

} // namespace process
