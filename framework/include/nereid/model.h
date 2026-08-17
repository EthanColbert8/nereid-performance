#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace nereid {

    enum class TensorDtype {
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

    TensorDtype StringToDtype(const std::string& dtype);
    std::string DtypeToString(TensorDtype dtype);
    size_t DtypeSizeBytes(TensorDtype dtype);

    // added to show the YAML parser how to make a TensorDtype
    inline TensorDtype StringToDtype_throws(const std::string& dtype) {
        if (dtype == "BOOL") { return TensorDtype::BOOL; }
        else if (dtype == "INT8") { return TensorDtype::INT8; }
        else if (dtype == "UINT8") { return TensorDtype::UINT8; }
        else if (dtype == "INT16") { return TensorDtype::INT16; }
        else if (dtype == "UINT16") { return TensorDtype::UINT16; }
        else if (dtype == "INT32") { return TensorDtype::INT32; }
        else if (dtype == "UINT32") { return TensorDtype::UINT32; }
        else if (dtype == "FP32") { return TensorDtype::FP32; }
        else if (dtype == "INT64") { return TensorDtype::INT64; }
        else if (dtype == "UINT64") { return TensorDtype::UINT64; }
        else if (dtype == "FP64") { return TensorDtype::FP64; }

        throw std::runtime_error("Unknown tensor dtype: " + dtype);
    }

} // namespace nereid


