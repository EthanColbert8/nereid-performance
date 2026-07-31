#include "nereid/service.h"
#include "utils/errors.h"

#include <chrono>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace nereid {

    size_t DatatypeSizeBytes(const std::string& datatype, std::string* error_message) {
        if (datatype == "FP32" || datatype == "INT32" || datatype == "UINT32") {
            return 4;
        }
        if (datatype == "FP64" || datatype == "INT64" || datatype == "UINT64") {
            return 8;
        }
        if (datatype == "INT16" || datatype == "UINT16") {
            return 2;
        }
        if (datatype == "INT8" || datatype == "UINT8" || datatype == "BOOL") {
            return 1;
        }

        utils::SetError(error_message, "unsupported tensor datatype: " + datatype);
        return 0;
    }

    bool WaitForServerReady(inference::GRPCInferenceService::Stub* stub, pid_t server_pid, int startup_timeout_secs, std::string* error_message) {
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
                utils::SetError(error_message, "server process exited before becoming ready");
                return false;
            }

            usleep(POLL_SLEEP_MICROSECONDS);
        }

        utils::SetError(error_message, "timed out waiting for server readiness");
        return false;
    }

} // namespace nereid
