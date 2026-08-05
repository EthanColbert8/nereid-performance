#pragma once

#include <string>
#include <vector>
#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace nereid {

    struct TensorSpec {
        std::string name;
        std::string datatype;
        std::vector<int64_t> shape;
    };

    struct ModelSpec {
        std::string name;
        std::string version;
        std::string platform;
        std::vector<TensorSpec> inputs;
        std::vector<TensorSpec> outputs;
    };

    bool LoadModelSpec(
        inference::GRPCInferenceService::Stub* stub,
        const char* model_name,
        ModelSpec* spec,
        std::string* error_message
    );
} // namespace nereid
