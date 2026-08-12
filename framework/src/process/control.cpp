#include "process/control.h"
#include "utils/errors.h"

#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace process {

    bool LaunchServer(const char* binary_path, ServerProcess* server, std::string* error_message) {
        pid_t pid = fork();
        if (pid < 0) {
            utils::SetError(error_message, "failed to fork server process");
            return false;
        }

        if (pid == 0) {
            execl(binary_path, binary_path, static_cast<char*>(nullptr));
            _exit(127);
        }

        server->pid = pid;
        server->owned = true;
        server->running = true;
        return true;
    }

    void StopServer(ServerProcess& server) {
        if (server.pid <= 0 || !server.owned || !server.running) { return; }

        kill(server.pid, SIGTERM);

        int status;
        for (int i = 0; i < 20; i++) {
            status = 0;
            const pid_t result = waitpid(server.pid, &status, WNOHANG);
            if (result == server.pid) { return; }

            usleep(100000);
        }

        kill(server.pid, SIGKILL);
        status = 0;
        waitpid(server.pid, &status, 0);

        server.running = false;
    }

} // namespace process
