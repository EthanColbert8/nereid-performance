#include "process/control.h"
#include "logging/logger.h"

#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace process {

    bool LaunchServer(const char* binary_path, ServerProcess& server, const char* log_path, logging::Logger& logger) {
        pid_t pid = fork();
        if (pid < 0) {
            logger.error("failed to fork server process");
            return false;
        }

        if (pid == 0) {
            int log_file = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (log_file >= 0) {
                dup2(log_file, STDOUT_FILENO);
                dup2(log_file, STDERR_FILENO);
                close(log_file);
            }

            execl(binary_path, binary_path, static_cast<char*>(nullptr));
            _exit(127);
        }

        server.pid = pid;
        server.owned = true;
        server.running = true;
        return true;
    }

    void StopServer(ServerProcess& server, logging::Logger& logger) {
        if (server.pid <= 0 || !server.owned || !server.running) { return; }

        kill(server.pid, SIGTERM);

        int status;
        for (int i = 0; i < 20; i++) {
            status = 0;
            const pid_t result = waitpid(server.pid, &status, WNOHANG);
            if (result == server.pid) { return; }

            usleep(100000);
        }

        logger.warning("Server process (PID %d) did not exit after SIGTERM, sending SIGKILL", server.pid);

        kill(server.pid, SIGKILL);
        status = 0;
        waitpid(server.pid, &status, 0);

        server.running = false;
    }

} // namespace process
