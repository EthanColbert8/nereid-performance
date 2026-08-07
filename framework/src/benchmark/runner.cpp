#include "benchmark/runner.h"
#include "benchmark/context.h"
#include "nereid/model.h"
#include "nereid/service.h"
#include "analysis/stats.h"
#include "utils/errors.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace benchmark {

    bool ValidateInferResponse(const nereid::ModelSpec& spec, const inference::ModelInferResponse& response, int batch_size, std::string* error_message) {
        if (response.outputs_size() != static_cast<int>(spec.outputs.size())) {
            utils::SetError(error_message, std::string("unexpected output count for model: ") + spec.name);
            return false;
        }

        for (int i = 0; i < response.outputs_size(); i++) {
            const auto& output = response.outputs(i);
            if (output.name() != spec.outputs[i].name) {
                utils::SetError(error_message, std::string("unexpected output tensor name for model: ") + spec.name);
                return false;
            }

            if (output.shape_size() > 0 && output.shape(0) != batch_size) {
                utils::SetError(error_message, std::string("unexpected output batch dimension for model: ") + spec.name);
                return false;
            }
        }

        return true;
    }

    bool RunBatchTrials(
        inference::GRPCInferenceService::Stub* stub,
        const nereid::ModelSpec& spec,
        int num_trials,
        int batch_size,
        analysis::RunningStats* latency_stats,
        analysis::RunningStats* throughput_stats,
        std::string* error_message
    ) {
        inference::ModelInferRequest request;
        request.set_model_name(spec.name);
        if (!spec.version.empty()) {
            request.set_model_version(spec.version);
        }

        for (size_t i = 0; i < spec.inputs.size(); i++) {
            const nereid::TensorSpec& input_spec = spec.inputs[i];
            
            auto* input = request.add_inputs();
            input->set_name(input_spec.name);
            input->set_datatype(input_spec.datatype);

            // Add shape values to the input one at a time for some reason...
            input->add_shape(batch_size);
            for (size_t j = 0; j < input_spec.shape.size(); j++) {
                input->add_shape(input_spec.shape[j]);
            }
        }

        for (size_t i = 0; i < spec.outputs.size(); i++) {
            auto* output = request.add_outputs();
            output->set_name(spec.outputs[i].name);
        }

        inference::ModelInferResponse response;
        grpc::ClientContext infer_context;

        // the tight loop around running inference trials
        for (int trial = 0; trial < num_trials; trial++) {
            request.clear_raw_input_contents();

            std::vector<std::string> raw_input_buffers;
            raw_input_buffers.reserve(spec.inputs.size());

            for (size_t i = 0; i < spec.inputs.size(); i++) {
                const nereid::TensorSpec& input_spec = spec.inputs[i];

                size_t element_count = batch_size;
                for (size_t j = 0; j < input_spec.shape.size(); j++) {
                    element_count *= static_cast<size_t>(input_spec.shape[j]);
                }
                const size_t bytes_per_element = nereid::DatatypeSizeBytes(input_spec.datatype, error_message);
                if (bytes_per_element == 0) {
                    return false;
                }

                // TODO (Ethan): Actually use random numbers, not zeros.
                std::string raw_contents(element_count * bytes_per_element, '\0');
                raw_input_buffers.push_back(std::move(raw_contents));
            }

            const auto start_time = std::chrono::steady_clock::now();

            for (size_t i = 0; i < raw_input_buffers.size(); i++) {
                request.add_raw_input_contents(raw_input_buffers[i]);
            }

            grpc::Status infer_status = stub->ModelInfer(&infer_context, request, &response);

            if (!infer_status.ok()) {
                char error_buf[400];
                std::snprintf(error_buf, sizeof(error_buf), "inference failed for model \"%s\". Error code: %d, message: %s", spec.name.c_str(), infer_status.error_code(), infer_status.error_message().c_str());
                utils::SetError(error_message, error_buf);
                return false;
            }

            const auto end_time = std::chrono::steady_clock::now();

            if (!ValidateInferResponse(spec, response, batch_size, error_message)) {
                return false;
            }

            double latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            double throughput = static_cast<double>(batch_size) / (latency_ms / 1000.0);

            analysis::RunningStatsPush(*latency_stats, latency_ms);
            analysis::RunningStatsPush(*throughput_stats, throughput);
        }

        return true;
    }

    bool RunBenchmark(const BenchmarkContext& ctx, nlohmann::json* report, std::string* error_message) {
        if (report == nullptr) {
            utils::SetError(error_message, "report output pointer is null");
            return false;
        }

        *report = nlohmann::json::object();

        auto channel = grpc::CreateChannel(ctx.server_address, grpc::InsecureChannelCredentials());
        auto stub = inference::GRPCInferenceService::NewStub(channel);

        grpc::ClientContext server_metadata_context;
        inference::ServerMetadataRequest server_metadata_request;
        inference::ServerMetadataResponse server_metadata_response;
        grpc::Status server_metadata_status = stub->ServerMetadata(&server_metadata_context, server_metadata_request, &server_metadata_response);
        if (!server_metadata_status.ok()) {
            utils::SetError(error_message, "failed to fetch server metadata");
            return false;
        }

        nlohmann::json report_models = nlohmann::json::array();
        nlohmann::json report_summary = nlohmann::json::array();

        for (size_t i = 0; i < ctx.model_count; i++) {
            const nereid::ModelSpec& spec = ctx.model_specs[i];
            nlohmann::json model_json = nlohmann::json::object();
            model_json["name"] = spec.name;
            model_json["version"] = spec.version;
            model_json["platform"] = spec.platform;

            nlohmann::json inputs_json = nlohmann::json::array();
            for (size_t j = 0; j < spec.inputs.size(); j++) {
                nlohmann::json tensor_json = nlohmann::json::object();
                tensor_json["name"] = spec.inputs[j].name;
                tensor_json["datatype"] = spec.inputs[j].datatype;
                tensor_json["shape"] = spec.inputs[j].shape;
                inputs_json.push_back(tensor_json);
            }
            model_json["inputs"] = inputs_json;

            nlohmann::json outputs_json = nlohmann::json::array();
            for (size_t j = 0; j < spec.outputs.size(); j++) {
                nlohmann::json tensor_json = nlohmann::json::object();
                tensor_json["name"] = spec.outputs[j].name;
                tensor_json["datatype"] = spec.outputs[j].datatype;
                tensor_json["shape"] = spec.outputs[j].shape;
                outputs_json.push_back(tensor_json);
            }
            model_json["outputs"] = outputs_json;
            report_models.push_back(model_json);

            for (int batch_index = 0; batch_index < ctx.batch_size_count; batch_index++) {
                const int batch_size = ctx.batch_sizes[batch_index];
                analysis::RunningStats latency_stats;
                analysis::RunningStats throughput_stats;
                analysis::RunningStatsInit(latency_stats);
                analysis::RunningStatsInit(throughput_stats);

                if (!RunBatchTrials(stub.get(), spec, ctx.num_trials, batch_size, &latency_stats, &throughput_stats, error_message)) {
                    return false;
                }

                nlohmann::json summary_row = nlohmann::json::object();
                summary_row["model_name"] = spec.name;
                summary_row["batch_size"] = batch_size;
                summary_row["num_trials"] = ctx.num_trials;
                summary_row["mean_latency_ms"] = analysis::RunningStatsMean(latency_stats);
                summary_row["std_latency_ms"] = analysis::RunningStatsStdDev(latency_stats);
                summary_row["stderr_latency_ms"] = analysis::RunningStatsStdErr(latency_stats);
                summary_row["mean_throughput_infer_per_sec"] = analysis::RunningStatsMean(throughput_stats);
                summary_row["std_throughput_infer_per_sec"] = analysis::RunningStatsStdDev(throughput_stats);
                summary_row["stderr_throughput_infer_per_sec"] = analysis::RunningStatsStdErr(throughput_stats);
                report_summary.push_back(summary_row);
            }
        }

        nlohmann::json config_json = nlohmann::json::object();
        config_json["num_trials"] = ctx.num_trials;
        // config_json["server_binary_path"] = ctx.server_binary_path;
        config_json["server_address"] = ctx.server_address;
        // config_json["output_path"] = ctx.output_path;

        nlohmann::json batch_sizes_json = nlohmann::json::array();
        for (int i = 0; i < ctx.batch_size_count; i++) {
            batch_sizes_json.push_back(ctx.batch_sizes[i]);
        }
        config_json["batch_sizes"] = batch_sizes_json;

        // nlohmann::json model_names_json = nlohmann::json::array();
        // for (int i = 0; i < ctx.model_count; i++) {
        //     model_names_json.push_back(ctx.model_names[i]);
        // }
        // config_json["model_names"] = model_names_json;

        nlohmann::json server_json = nlohmann::json::object();
        server_json["name"] = server_metadata_response.name();
        server_json["version"] = server_metadata_response.version();
        nlohmann::json extensions_json = nlohmann::json::array();
        for (int i = 0; i < server_metadata_response.extensions_size(); i++) {
            extensions_json.push_back(server_metadata_response.extensions(i));
        }
        server_json["extensions"] = extensions_json;

        (*report)["config"] = config_json;
        (*report)["server"] = server_json;
        (*report)["models"] = report_models;
        (*report)["summary"] = report_summary;

        return true;
    }

} // namespace benchmark