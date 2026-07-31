#pragma once

#include <string>
#include <sys/types.h>
#include <unistd.h>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace nereid {

    size_t DatatypeSizeBytes(const std::string& datatype, std::string* error_message)
    bool WaitForServerReady(inference::GRPCInferenceService::Stub* stub, pid_t server_pid, int startup_timeout_secs, std::string* error_message);

} // namespace nereid
