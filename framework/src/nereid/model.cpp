#include "nereid/model.h"

#include <string>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace nereid {

    TensorDtype StringToDtype(const std::string& dtype) {
        if (dtype == "BOOL") { return BOOL; }
        else if (dtype == "INT8") { return INT8; }
        else if (dtype == "UINT8") { return UINT8; }
        else if (dtype == "INT16") { return INT16; }
        else if (dtype == "UINT16") { return UINT16; }
        else if (dtype == "INT32") { return INT32; }
        else if (dtype == "UINT32") { return UINT32; }
        else if (dtype == "FP32") { return FP32; }
        else if (dtype == "INT64") { return INT64; }
        else if (dtype == "UINT64") { return UINT64; }
        else if (dtype == "FP64") { return FP64; }
        else { return INVALID; }
    }

    std::string DtypeToString(TensorDtype dtype) {
        switch (dtype) {
            case BOOL: return "BOOL";
            case INT8: return "INT8";
            case UINT8: return "UINT8";
            case INT16: return "INT16";
            case UINT16: return "UINT16";
            case INT32: return "INT32";
            case UINT32: return "UINT32";
            case FP32: return "FP32";
            case INT64: return "INT64";
            case UINT64: return "UINT64";
            case FP64: return "FP64";
            default: return "INVALID";
        }
    }

    size_t DtypeSizeBytes(TensorDtype dtype) {
        switch (dtype) {
            case BOOL:
            case INT8:
            case UINT8:
                return 1;

            case INT16:
            case UINT16:
                return 2;

            case INT32:
            case UINT32:
            case FP32:
                return 4;

            case INT64:
            case UINT64:
            case FP64:
                return 8;

            default:
                return 0;
        }
    }

} // namespace nereid