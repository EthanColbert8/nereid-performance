#pragma once

#include <string>
#include <vector>
#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace nereid {

    enum TensorDtype {
        INVALID,
        BOOL,
        INT8,
        UINT8,
        INT16,
        UINT16,
        INT32,
        UINT32,
        FP32,
        INT64,
        UINT64,
        FP64,
    };

    struct TensorSpec {
        std::string name;
        TensorDtype dtype;
        std::vector<int64_t> shape;
    };

    struct ModelSpec {
        std::string name;
        std::string version;
        std::string platform;
        std::vector<TensorSpec> inputs;
        std::vector<TensorSpec> outputs;
    };

    TensorDtype StringToDtype(const std::string& dtype, std::string* error_message);
    std::string DtypeToString(TensorDtype dtype);
    size_t DtypeSizeBytes(TensorDtype dtype);

    bool LoadModelSpec(
        inference::GRPCInferenceService::Stub* stub,
        const char* model_name,
        ModelSpec* spec,
        std::string* error_message
    );
} // namespace nereid
