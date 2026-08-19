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

    constexpr int64_t DEFAULT_DYNAMIC_SIZE = 16;

    // TODO (Ethan): check equality of tensor names in here...
    bool MergeInputShape(
        const nereid::TensorSpec& config_input,
        const inference::ModelMetadataResponse::TensorMetadata& server_input,
        nereid::TensorSpec& merged_input,
        logging::Logger& logger
    ) {
        nereid::TensorDtype server_dtype = nereid::StringToDtype(server_input.datatype());
        if (server_dtype == nereid::TensorDtype::INVALID) {
            logger.error("invalid input datatype for model \"%s\": \"%s\"", server_input.name().c_str(), server_input.datatype().c_str());
            return false;
        }

        // No shape in config, use server's spec and default dynamic size
        // (we use nothing from config if it doesn't provide a shape).
        if (config_input.shape.size() == 0) {
            merged_input.name = server_input.name();
            merged_input.dtype = server_dtype;
            merged_input.shape.clear();
            merged_input.shape.reserve(server_input.shape_size());

            if (server_input.shape(0) >= 1) {
                merged_input.shape.push_back(server_input.shape(0));
            }

            for (size_t i = 1; i < server_input.shape_size(); i++) {
                int64_t dim = server_input.shape(i);

                if (dim >= 1) {
                    merged_input.shape.push_back(dim);
                }
                else {
                    merged_input.shape.push_back(DEFAULT_DYNAMIC_SIZE);
                }
            }
            return true;
        }

        // Error if config and server dtypes mismatch
        if (server_dtype != config_input.dtype) {
            logger.error(
                "input dtype mismatch for model \"%s\": server reported %s, config provided %s",
                server_input.name().c_str(), server_input.datatype().c_str(), nereid::DtypeToString(config_input.dtype).c_str()
            );
            return false;
        }

        const int server_shape_count = server_input.shape_size();
        const int config_shape_count = config_input.shape.size();

        const bool has_batch_dim = (server_shape_count == config_shape_count + 1) && (server_input.shape(0) == -1);
        if ((!has_batch_dim) && (server_shape_count != config_shape_count)) {
            logger.error(
                "input shape rank mismatch for model \"%s\": server reported %d dims, config provided %d dims",
                config_input.name, server_shape_count, config_shape_count
            );
            return false;
        }

        merged_input.name = server_input.name();
        merged_input.dtype = server_dtype;
        merged_input.shape.clear();
        merged_input.shape.reserve(config_shape_count);

        int server_shape_idx = has_batch_dim ? 1 : 0;
        int config_shape_idx = 0;
        while (config_shape_idx < config_shape_count) {
            int64_t server_shape_val = server_input.shape(server_shape_idx);
            int64_t config_shape_val = config_input.shape[config_shape_idx];

            if (config_shape_val < 1) {
                // NOTE (Ethan): this means we can still error after altering data in merged_input...
                //               should try to avoid that maybe?
                logger.error(
                    "invalid config shape value for model \"%s\": got %d while values must be >=1",
                    server_input.name(), config_shape_val
                );
                return false;
            }

            if (server_shape_val >= 1) {
                if (server_shape_val != config_shape_val) {
                    logger.error(
                        "input shpae value mismatch for model \"%s\": server reported %d, config provided %d",
                        server_input.name(), server_shape_val, config_shape_val
                    );
                    return false;
                }
                merged_input.shape.push_back(server_shape_val);
            }
            else {
                merged_input.shape.push_back(config_shape_val);
            }

            server_shape_idx++;
            config_shape_idx++;
        }
        return true;
    }

    bool BuildBenchmarkContext(const config::Settings& args, BenchmarkContext& context, logging::Logger& logger) {
        if (args.model_specs.size() < 1) {
            logger.error("no benchmark models were provided");
            return false;
        }

        if (args.batch_sizes.size() < 1) {
            logger.error("no batch sizes were provided");
            return false;
        }
        
        static std::string combined_server_address;
        if (!utils::BuildAddress(args.server_address.c_str(), args.server_port, &combined_server_address)) {
            logger.error("invalid server address and/or port: %s:%d", args.server_address.c_str(), args.server_port);
            return false;
        }

        context.server_address = combined_server_address;
        context.num_trials = args.num_trials;
        context.batch_sizes = args.batch_sizes;

        auto channel = grpc::CreateChannel(context.server_address, grpc::InsecureChannelCredentials());
        auto stub = inference::GRPCInferenceService::NewStub(channel);

        // TODO (Ethan): discover models on server if not configured
        if (args.model_specs.size() < 1) {
            logger.error("no models found in run configuration");
            return false;
        }

        context.model_specs.clear();
        context.model_specs.reserve(args.model_specs.size());

        for (size_t model_index = 0; model_index < args.model_specs.size(); model_index++) {
            const nereid::ModelSpec& config_spec = args.model_specs[model_index];

            grpc::ClientContext metadata_context;
            inference::ModelMetadataRequest metadata_request;
            inference::ModelMetadataResponse metadata_response;
            metadata_request.set_name(config_spec.name);

            const grpc::Status metadata_status = stub->ModelMetadata(&metadata_context, metadata_request, &metadata_response);
            if (!metadata_status.ok()) {
                if (metadata_status.error_code() == grpc::StatusCode::NOT_FOUND) {
                    logger.warning("Skipping model \"%s\" because it does not exist on the server", config_spec.name.c_str());
                    continue;
                }

                logger.error("failed to fetch metadata for model \"%s\": %s", config_spec.name.c_str(), metadata_status.error_message().c_str());
                return false;
            }

            if (metadata_response.inputs_size() != config_spec.inputs.size()) {
                logger.error(
                    "input count mismatch for model \"%s\": server reported %d, CLI provided %d",
                    config_spec.name.c_str(), metadata_response.inputs_size(), config_spec.inputs.size()
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
                
                // TODO (Ethan): match input tensor names, don't assume config has same order as server
                if (!MergeInputShape(config_spec.inputs[input_index], metadata_response.inputs(input_index), merged_input, logger)) {
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
                    logger.error("invalid output datatype for model \"%s\": \"%s\"", config_spec.name.c_str(), output.datatype().c_str());
                    return false;
                }

                merged_output.shape.reserve(static_cast<size_t>(output.shape_size()));
                for (int shape_index = 0; shape_index < output.shape_size(); shape_index++) {
                    merged_output.shape.push_back(output.shape(shape_index));
                }
                merged_spec.outputs.push_back(std::move(merged_output));
            }

            if (merged_spec.inputs.empty() || merged_spec.outputs.empty()) {
                logger.error("metadata for model \"%s\" missing inputs or outputs", config_spec.name.c_str());
                return false;
            }

            context.model_specs.push_back(std::move(merged_spec));
        }

        if (context.model_specs.empty()) {
            logger.error("no desired models were found on the server");
            return false;
        }

        return true;
    }

} // namespace benchmark
