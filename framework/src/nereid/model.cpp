#include "nereid/model.h"

#include <string>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace nereid {

    TensorDtype StringToDtype(const std::string& dtype) {
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
        else { return TensorDtype::INVALID; }
    }

    std::string DtypeToString(TensorDtype dtype) {
        switch (dtype) {
            case TensorDtype::BOOL: return "BOOL";
            case TensorDtype::INT8: return "INT8";
            case TensorDtype::UINT8: return "UINT8";
            case TensorDtype::INT16: return "INT16";
            case TensorDtype::UINT16: return "UINT16";
            case TensorDtype::INT32: return "INT32";
            case TensorDtype::UINT32: return "UINT32";
            case TensorDtype::FP32: return "FP32";
            case TensorDtype::INT64: return "INT64";
            case TensorDtype::UINT64: return "UINT64";
            case TensorDtype::FP64: return "FP64";
            default: return "INVALID";
        }
    }

    size_t DtypeSizeBytes(TensorDtype dtype) {
        switch (dtype) {
            case TensorDtype::BOOL:
            case TensorDtype::INT8:
            case TensorDtype::UINT8:
                return 1;

            case TensorDtype::INT16:
            case TensorDtype::UINT16:
                return 2;

            case TensorDtype::INT32:
            case TensorDtype::UINT32:
            case TensorDtype::FP32:
                return 4;

            case TensorDtype::INT64:
            case TensorDtype::UINT64:
            case TensorDtype::FP64:
                return 8;

            default:
                return 0;
        }
    }

} // namespace nereid