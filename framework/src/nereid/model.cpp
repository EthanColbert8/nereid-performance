#include "nereid/model.h"
#include "utils/errors.h"

#include <string>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace nereid {

    bool LoadModelSpec(
        inference::GRPCInferenceService::Stub* stub,
        const char* model_name,
        ModelSpec* spec,
        std::string* error_message
    ) {
        grpc::ClientContext ready_context;
        inference::ModelReadyRequest ready_request;
        inference::ModelReadyResponse ready_response;
        ready_request.set_name(model_name);

        grpc::Status ready_status = stub->ModelReady(&ready_context, ready_request, &ready_response);
        if (!ready_status.ok() || !ready_response.ready()) {
            utils::SetError(error_message, std::string("model not ready: ") + model_name);
            return false;
        }

        grpc::ClientContext metadata_context;
        inference::ModelMetadataRequest metadata_request;
        inference::ModelMetadataResponse metadata_response;
        metadata_request.set_name(model_name);

        grpc::Status metadata_status = stub->ModelMetadata(&metadata_context, metadata_request, &metadata_response);
        if (!metadata_status.ok()) {
            utils::SetError(error_message, std::string("failed to fetch metadata for model: ") + model_name);
            return false;
        }

        spec->name = metadata_response.name();
        spec->version.clear();
        if (metadata_response.versions_size() > 0) {
            spec->version = metadata_response.versions(0);
        }
        spec->platform = metadata_response.platform();
        spec->inputs.clear();
        spec->outputs.clear();

        for (int i = 0; i < metadata_response.inputs_size(); i++) {
            const auto& input = metadata_response.inputs(i);
            TensorSpec tensor = {};
            tensor.name = input.name();
            tensor.datatype = input.datatype();
            for (int j = 0; j < input.shape_size(); j++) {
                tensor.shape.push_back(input.shape(j));
            }
            spec->inputs.push_back(tensor);
        }

        for (int i = 0; i < metadata_response.outputs_size(); i++) {
            const auto& output = metadata_response.outputs(i);
            TensorSpec tensor = {};
            tensor.name = output.name();
            tensor.datatype = output.datatype();
            for (int j = 0; j < output.shape_size(); j++) {
                tensor.shape.push_back(output.shape(j));
            }
            spec->outputs.push_back(tensor);
        }

        if (spec->inputs.empty() || spec->outputs.empty()) {
            utils::SetError(error_message, std::string("model metadata missing inputs or outputs: ") + model_name);
            return false;
        }

        return true;
    }

} // namespace nereid