#include "nereid/service.h"
#include "utils/address.h"
#include "logging/logger.h"

#include <chrono>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace nereid {

    constexpr int POLL_SLEEP_MICROSECONDS = 250000; // 250 milliseconds

    bool WaitForServerReady(const char* server_address, int server_port, pid_t server_pid, int startup_timeout_secs, logging::Logger& logger) {
        std::string combined_server_address;
        if (!utils::BuildAddress(server_address, server_port, &combined_server_address)) {
            logger.error("invalid server address and/or port");
            return false;
        }

        auto channel = grpc::CreateChannel(combined_server_address.c_str(), grpc::InsecureChannelCredentials());
        auto stub = inference::GRPCInferenceService::NewStub(channel);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(startup_timeout_secs);

        while (std::chrono::steady_clock::now() < deadline) {
            grpc::ClientContext live_context;
            grpc::ClientContext ready_context;
            inference::ServerLiveResponse live_response;
            inference::ServerReadyResponse ready_response;

            const grpc::Status live_status = stub->ServerLive(&live_context, inference::ServerLiveRequest{}, &live_response);
            const grpc::Status ready_status = stub->ServerReady(&ready_context, inference::ServerReadyRequest{}, &ready_response);

            if (live_status.ok() && ready_status.ok() && live_response.live() && ready_response.ready()) {
                return true;
            }

            int status = 0;
            const pid_t child_result = waitpid(server_pid, &status, WNOHANG);
            if (child_result > 0) {
                logger.error("server process exited before becoming ready");
                return false;
            }

            usleep(POLL_SLEEP_MICROSECONDS);
        }

        logger.error("timed out waiting for server readiness");
        return false;
    }

    bool WaitForServerReady(const char* server_address, int server_port, int startup_timeout_secs, logging::Logger& logger) {
        std::string combined_server_address;
        if (!utils::BuildAddress(server_address, server_port, &combined_server_address)) {
            logger.error("invalid server address and/or port");
            return false;
        }

        auto channel = grpc::CreateChannel(combined_server_address.c_str(), grpc::InsecureChannelCredentials());
        auto stub = inference::GRPCInferenceService::NewStub(channel);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(startup_timeout_secs);

        while (std::chrono::steady_clock::now() < deadline) {
            grpc::ClientContext live_context;
            grpc::ClientContext ready_context;
            inference::ServerLiveResponse live_response;
            inference::ServerReadyResponse ready_response;

            const grpc::Status live_status = stub->ServerLive(&live_context, inference::ServerLiveRequest{}, &live_response);
            const grpc::Status ready_status = stub->ServerReady(&ready_context, inference::ServerReadyRequest{}, &ready_response);

            if (live_status.ok() && ready_status.ok() && live_response.live() && ready_response.ready()) {
                return true;
            }

            usleep(POLL_SLEEP_MICROSECONDS);
        }

        logger.error("timed out waiting for server readiness");
        return false;
    }

} // namespace nereid
