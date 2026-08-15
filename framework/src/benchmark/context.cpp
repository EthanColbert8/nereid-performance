#include "benchmark/context.h"

#include "config/settings.h"
#include "logging/logger.h"
#include "nereid/model.h"
#include "utils/address.h"

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

// HERE: This guy needs major updating!!

namespace benchmark {

    bool MergeInputShape(
        const config::PartialModelSpec& partial_model_spec,
        int input_index,
        const inference::ModelMetadataResponse::TensorMetadata& server_input,
        nereid::TensorSpec* merged_input,
        logging::Logger& logger
    ) {
        if (input_index < 0 || input_index >= partial_model_spec.input_count) {
            logger.error("internal error: input index out of range while building benchmark context");
            return false;
        }

        if (partial_model_spec.input_shapes == nullptr || partial_model_spec.input_shape_counts == nullptr) {
            logger.error("invalid partial model spec for model \"%s\"", partial_model_spec.name);
            return false;
        }

        const int64_t* cli_shape = partial_model_spec.input_shapes[input_index];
        const int cli_shape_count = partial_model_spec.input_shape_counts[input_index];
        if (cli_shape == nullptr || cli_shape_count <= 0) {
            logger.error("invalid input shape for model \"%s\"", partial_model_spec.name);
            return false;
        }

        const int server_shape_count = server_input.shape_size();
        const bool has_batch_dimension = (server_shape_count == cli_shape_count + 1 && server_input.shape(0) == -1);
        if (!has_batch_dimension && server_shape_count != cli_shape_count) {
            logger.error(
                "input shape rank mismatch for model \"%s\": server reported %d dims, CLI provided %d dims",
                partial_model_spec.name, server_shape_count, cli_shape_count
            );
            return false;
        }

        merged_input->dtype = nereid::StringToDtype(server_input.datatype());
        if (merged_input->dtype == nereid::TensorDtype::INVALID) {
            logger.error("invalid input datatype for model \"%s\": \"%s\"", partial_model_spec.name, server_input.datatype().c_str());
            return false;
        }

        merged_input->name = server_input.name();
        merged_input->shape.clear();
        merged_input->shape.reserve(static_cast<size_t>(cli_shape_count));

        const int server_start_index = has_batch_dimension ? 1 : 0;
        const int cli_start_index = 0;
        for (int server_index = server_start_index, cli_index = cli_start_index; server_index < server_shape_count; server_index++, cli_index++) {
            const int64_t server_dim = server_input.shape(server_index);
            const int64_t cli_dim = cli_shape[cli_index];

            if (cli_dim <= 0) {
                logger.error("CLI shape for model \"%s\" contains a non-positive dimension (%d)", partial_model_spec.name, cli_dim);
                return false;
            }

            if (server_dim == -1) {
                merged_input->shape.push_back(cli_dim);
                continue;
            }

            if (server_dim != cli_dim) {
                logger.error("input shape mismatch for model \"%s\": server dim %d, CLI dim %d", partial_model_spec.name, server_dim, cli_dim);
                return false;
            }

            merged_input->shape.push_back(server_dim);
        }

        return true;
    }

    bool BuildBenchmarkContext(const config::Settings& args, BenchmarkContext* context, logging::Logger& logger) {
        if (context == nullptr) {
            logger.error("benchmark context pointer is null");
            return false;
        }

        static std::string combined_server_address;
        if (!utils::BuildAddress(args.server_address.c_str(), args.server_port, &combined_server_address)) {
            logger.error("invalid server address and/or port");
            return false;
        }

        if (args.model_specs.size() < 1) {
            logger.error("no benchmark models were provided");
            return false;
        }

        if (args.batch_sizes.size() < 1) {
            logger.error("no batch sizes were provided");
            return false;
        }

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
                logger.error("encountered an unnamed model in the CLI args");
                return false;
            }

            grpc::ClientContext metadata_context;
            inference::ModelMetadataRequest metadata_request;
            inference::ModelMetadataResponse metadata_response;
            metadata_request.set_name(partial_model_spec.name);

            const grpc::Status metadata_status = stub->ModelMetadata(&metadata_context, metadata_request, &metadata_response);
            if (!metadata_status.ok()) {
                if (metadata_status.error_code() == grpc::StatusCode::NOT_FOUND) {
                    logger.warning("Skipping model \"%s\" because it does not exist on the server", partial_model_spec.name);
                    continue;
                }

                logger.error("failed to fetch metadata for model \"%s\": %s", partial_model_spec.name, metadata_status.error_message().c_str());
                return false;
            }

            if (metadata_response.inputs_size() != partial_model_spec.input_count) {
                logger.error(
                    "input count mismatch for model \"%s\": server reported %d, CLI provided %d",
                    partial_model_spec.name, metadata_response.inputs_size(), partial_model_spec.input_count
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
                if (!MergeInputShape(partial_model_spec, input_index, metadata_response.inputs(input_index), &merged_input, logger)) {
                    return false;
                }

                merged_spec.inputs.push_back(std::move(merged_input));
            }

            merged_spec.outputs.reserve(static_cast<size_t>(metadata_response.outputs_size()));
            for (int output_index = 0; output_index < metadata_response.outputs_size(); output_index++) {
                const auto& output = metadata_response.outputs(output_index);
                nereid::TensorSpec merged_output = {};
                merged_output.name = output.name();
                
                merged_output.dtype = nereid::StringToDtype(output.datatype());
                if (merged_output.dtype == nereid::TensorDtype::INVALID) {
                    logger.error("invalid output datatype for model \"%s\": \"%s\"", partial_model_spec.name, output.datatype().c_str());
                    return false;
                }

                merged_output.shape.reserve(static_cast<size_t>(output.shape_size()));
                for (int shape_index = 0; shape_index < output.shape_size(); shape_index++) {
                    merged_output.shape.push_back(output.shape(shape_index));
                }
                merged_spec.outputs.push_back(std::move(merged_output));
            }

            if (merged_spec.inputs.empty() || merged_spec.outputs.empty()) {
                logger.error("metadata for model \"%s\" missing inputs or outputs", partial_model_spec.name);
                return false;
            }

            merged_model_specs.push_back(std::move(merged_spec));
        }

        if (merged_model_specs.empty()) {
            logger.error("no desired models were found on the server");
            return false;
        }

        context->model_specs = merged_model_specs.data();
        context->model_count = static_cast<int>(merged_model_specs.size());

        return true;
    }

} // namespace benchmark

