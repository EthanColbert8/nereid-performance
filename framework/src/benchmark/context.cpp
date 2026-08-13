#include "benchmark/context.h"

#include "cli/args.h"
#include "logging/logger.h"
#include "nereid/model.h"
#include "utils/address.h"
#include "utils/errors.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <sys/types.h>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace benchmark {

    bool MergeInputShape(
        const cli::PartialModelSpec& partial_model_spec,
        int input_index,
        const inference::ModelMetadataResponse::TensorMetadata& server_input,
        nereid::TensorSpec* merged_input,
        std::string* error_message
    ) {
        if (input_index < 0 || input_index >= partial_model_spec.input_count) {
            utils::SetError(error_message, "internal error: input index out of range while building benchmark context");
            return false;
        }

        if (partial_model_spec.input_shapes == nullptr || partial_model_spec.input_shape_counts == nullptr) {
            utils::SetError(error_message, std::string("invalid partial model spec for model: ") + partial_model_spec.name);
            return false;
        }

        const int64_t* cli_shape = partial_model_spec.input_shapes[input_index];
        const int cli_shape_count = partial_model_spec.input_shape_counts[input_index];
        if (cli_shape == nullptr || cli_shape_count <= 0) {
            utils::SetError(error_message, std::string("invalid input shape for model: ") + partial_model_spec.name);
            return false;
        }

        const int server_shape_count = server_input.shape_size();
        const bool has_batch_dimension = (server_shape_count == cli_shape_count + 1 && server_input.shape(0) == -1);
        if (!has_batch_dimension && server_shape_count != cli_shape_count) {
            utils::SetError(
                error_message,
                std::string("input shape rank mismatch for model: ") + partial_model_spec.name +
                    " (server reported " + std::to_string(server_shape_count) +
                    " dims, CLI provided " + std::to_string(cli_shape_count) + ")"
            );
            return false;
        }

        nereid::TensorDtype dtype = nereid::StringToDtype(server_input.datatype(), error_message);
        if (dtype == nereid::TensorDtype::INVALID) { return false; }

        merged_input->name = server_input.name();
        merged_input->dtype = dtype;
        merged_input->shape.clear();
        merged_input->shape.reserve(static_cast<size_t>(cli_shape_count));

        const int server_start_index = has_batch_dimension ? 1 : 0;
        const int cli_start_index = 0;
        for (int server_index = server_start_index, cli_index = cli_start_index; server_index < server_shape_count; server_index++, cli_index++) {
            const int64_t server_dim = server_input.shape(server_index);
            const int64_t cli_dim = cli_shape[cli_index];

            if (cli_dim <= 0) {
                utils::SetError(error_message, std::string("CLI shape contains a non-positive dimension for model: ") + partial_model_spec.name);
                return false;
            }

            if (server_dim == -1) {
                merged_input->shape.push_back(cli_dim);
                continue;
            }

            if (server_dim != cli_dim) {
                utils::SetError(
                    error_message,
                    std::string("input shape mismatch for model: ") + partial_model_spec.name +
                        " (server dim " + std::to_string(server_dim) +
                        ", CLI dim " + std::to_string(cli_dim) + ")"
                );
                return false;
            }

            merged_input->shape.push_back(server_dim);
        }

        return true;
    }

    bool BuildBenchmarkContext(const cli::Args& args, BenchmarkContext* context, logging::Logger* logger, std::string* error_message) {
        if (context == nullptr) {
            utils::SetError(error_message, "benchmark context output pointer is null");
            return false;
        }

        static std::string combined_server_address;
        if (!utils::BuildAddress(args.server_address, args.server_port, &combined_server_address, error_message)) {
            return false;
        }

        if (args.model_specs == nullptr || args.model_count <= 0) {
            utils::SetError(error_message, "no benchmark models were provided");
            return false;
        }

        if (args.batch_sizes == nullptr || args.batch_size_count <= 0) {
            utils::SetError(error_message, "no batch sizes were provided");
            return false;
        }

        context->logger = logger;
        context->server_address = combined_server_address.c_str();
        context->batch_sizes = args.batch_sizes;
        context->batch_size_count = args.batch_size_count;
        context->num_trials = args.num_trials;

        auto channel = grpc::CreateChannel(context->server_address, grpc::InsecureChannelCredentials());
        auto stub = inference::GRPCInferenceService::NewStub(channel);

        static std::vector<nereid::ModelSpec> merged_model_specs;
        merged_model_specs.clear();
        merged_model_specs.reserve(static_cast<size_t>(args.model_count));

        for (int model_index = 0; model_index < args.model_count; model_index++) {
            const cli::PartialModelSpec& partial_model_spec = args.model_specs[model_index];
            if (partial_model_spec.name == nullptr || *partial_model_spec.name == '\0') {
                utils::SetError(error_message, "encountered an unnamed model in the CLI args");
                return false;
            }

            grpc::ClientContext metadata_context;
            inference::ModelMetadataRequest metadata_request;
            inference::ModelMetadataResponse metadata_response;
            metadata_request.set_name(partial_model_spec.name);

            const grpc::Status metadata_status = stub->ModelMetadata(&metadata_context, metadata_request, &metadata_response);
            if (!metadata_status.ok()) {
                if (metadata_status.error_code() == grpc::StatusCode::NOT_FOUND) {
                    logger->warning("Skipping model \"%s\" because it does not exist on the server", partial_model_spec.name);
                    continue;
                }

                utils::SetError(
                    error_message,
                    std::string("failed to fetch metadata for model: ") + partial_model_spec.name +
                        " (" + metadata_status.error_message() + ")"
                );
                return false;
            }

            if (metadata_response.inputs_size() != partial_model_spec.input_count) {
                utils::SetError(
                    error_message,
                    std::string("input count mismatch for model: ") + partial_model_spec.name +
                        " (server reported " + std::to_string(metadata_response.inputs_size()) +
                        ", CLI provided " + std::to_string(partial_model_spec.input_count) + ")"
                );
                return false;
            }

            nereid::ModelSpec merged_spec = {};
            merged_spec.name = metadata_response.name();
            merged_spec.version.clear();
            if (metadata_response.versions_size() > 0) {
                // NOTE (Ethan): Should this be max, not first?
                merged_spec.version = metadata_response.versions(0);
            }
            merged_spec.platform = metadata_response.platform();

            merged_spec.inputs.reserve(static_cast<size_t>(metadata_response.inputs_size()));
            for (int input_index = 0; input_index < metadata_response.inputs_size(); input_index++) {
                nereid::TensorSpec merged_input = {};

                // TODO (Ethan): match server and CLI input names, don't assume they're in same order
                if (!MergeInputShape(partial_model_spec, input_index, metadata_response.inputs(input_index), &merged_input, error_message)) {
                    return false;
                }

                merged_spec.inputs.push_back(std::move(merged_input));
            }

            merged_spec.outputs.reserve(static_cast<size_t>(metadata_response.outputs_size()));
            for (int output_index = 0; output_index < metadata_response.outputs_size(); output_index++) {
                const auto& output = metadata_response.outputs(output_index);
                nereid::TensorSpec merged_output = {};
                merged_output.name = output.name();
                
                nereid::TensorDtype dtype = nereid::StringToDtype(output.datatype(), error_message);
                if (dtype == nereid::TensorDtype::INVALID) { return false; }
                merged_output.dtype = dtype;

                merged_output.shape.reserve(static_cast<size_t>(output.shape_size()));
                for (int shape_index = 0; shape_index < output.shape_size(); shape_index++) {
                    merged_output.shape.push_back(output.shape(shape_index));
                }
                merged_spec.outputs.push_back(std::move(merged_output));
            }

            if (merged_spec.inputs.empty() || merged_spec.outputs.empty()) {
                utils::SetError(error_message, std::string("model metadata missing inputs or outputs: ") + partial_model_spec.name);
                return false;
            }

            merged_model_specs.push_back(std::move(merged_spec));
        }

        if (merged_model_specs.empty()) {
            utils::SetError(error_message, "no desired models were found on the server");
            return false;
        }

        context->model_specs = merged_model_specs.data();
        context->model_count = static_cast<int>(merged_model_specs.size());

        return true;
    }

} // namespace benchmark

