#include "benchmark/runner.h"

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

    namespace {

        constexpr int STARTUP_TIMEOUT_SECONDS = 120;
        constexpr int POLL_SLEEP_MICROSECONDS = 250000;

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

        struct RunningStats {
            int count;
            double mean;
            double m2;
        };

        struct ServerProcess {
            pid_t pid;
        };

        void StatsInit(RunningStats& stats) {
            stats.count = 0;
            stats.mean = 0.0;
            stats.m2 = 0.0;
        }

        void StatsPush(RunningStats& stats, double value) {
            stats.count++;
            const double delta = value - stats.mean;
            stats.mean += delta / static_cast<double>(stats.count);
            const double delta2 = value - stats.mean;
            stats.m2 += delta * delta2;
        }

        double StatsStdDev(const RunningStats& stats) {
            if (stats.count < 2) {
                return 0.0;
            }

            return std::sqrt(stats.m2 / static_cast<double>(stats.count - 1));
        }

        void SetError(std::string* error_message, const char* message) {
            if (error_message != nullptr) {
                *error_message = message;
            }
        }

        void SetError(std::string* error_message, const std::string& message) {
            if (error_message != nullptr) {
                *error_message = message;
            }
        }

        bool LaunchServer(const char* server_binary_path, ServerProcess* server, std::string* error_message) {
            pid_t pid = fork();
            if (pid < 0) {
                SetError(error_message, "failed to fork server process");
                return false;
            }

            if (pid == 0) {
                execl(server_binary_path, server_binary_path, static_cast<char*>(nullptr));
                _exit(127);
            }

            server->pid = pid;
            return true;
        }

        void StopServer(const ServerProcess& server) {
            if (server.pid <= 0) {
                return;
            }

            kill(server.pid, SIGTERM);

            for (int i = 0; i < 20; i++) {
                int status = 0;
                const pid_t result = waitpid(server.pid, &status, WNOHANG);
                if (result == server.pid) {
                    return;
                }

                usleep(100000);
            }

            kill(server.pid, SIGKILL);
            int status = 0;
            waitpid(server.pid, &status, 0);
        }

        bool WaitForServerReady(inference::GRPCInferenceService::Stub* stub, pid_t server_pid, std::string* error_message) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(STARTUP_TIMEOUT_SECONDS);

            while (std::chrono::steady_clock::now() < deadline) {
                grpc::ClientContext live_context;
                grpc::ClientContext ready_context;
                inference::ServerLiveResponse live_response;
                inference::ServerReadyResponse ready_response;

                const grpc::Status live_status = stub->ServerLive(&live_context, inference::ServerLiveRequest{}, &live_response);
                const grpc::Status ready_status = stub->ServerReady(&ready_context, inference::ServerReadyRequest{}, &ready_response);

                if (live_status.ok() && ready_status.ok() && live_response.live() && ready_response.ready()) {
                    return true;
                }

                int status = 0;
                const pid_t child_result = waitpid(server_pid, &status, WNOHANG);
                if (child_result > 0) {
                    SetError(error_message, "server process exited before becoming ready");
                    return false;
                }

                usleep(POLL_SLEEP_MICROSECONDS);
            }

            SetError(error_message, "timed out waiting for server readiness");
            return false;
        }

        size_t DatatypeSizeBytes(const std::string& datatype, std::string* error_message) {
            if (datatype == "FP32" || datatype == "INT32" || datatype == "UINT32") {
                return 4;
            }
            if (datatype == "FP64" || datatype == "INT64" || datatype == "UINT64") {
                return 8;
            }
            if (datatype == "INT16" || datatype == "UINT16") {
                return 2;
            }
            if (datatype == "INT8" || datatype == "UINT8" || datatype == "BOOL") {
                return 1;
            }

            SetError(error_message, "unsupported tensor datatype: " + datatype);
            return 0;
        }

        bool BuildRequestShape(const std::vector<int64_t>& metadata_shape, int batch_size, std::vector<int64_t>* shape) {
            shape->clear();
            shape->push_back(batch_size);
            for (size_t i = 0; i < metadata_shape.size(); i++) {
                const int64_t dim = metadata_shape[i];
                shape->push_back(dim > 0 ? dim : 1);
            }
            return true;
        }

        bool LoadModelSpec(
            inference::GRPCInferenceService::Stub* stub,
            const char* model_name,
            ModelSpec* spec,
            std::string* error_message) {
            grpc::ClientContext ready_context;
            inference::ModelReadyRequest ready_request;
            inference::ModelReadyResponse ready_response;
            ready_request.set_name(model_name);

            grpc::Status ready_status = stub->ModelReady(&ready_context, ready_request, &ready_response);
            if (!ready_status.ok() || !ready_response.ready()) {
                SetError(error_message, std::string("model not ready: ") + model_name);
                return false;
            }

            grpc::ClientContext metadata_context;
            inference::ModelMetadataRequest metadata_request;
            inference::ModelMetadataResponse metadata_response;
            metadata_request.set_name(model_name);

            grpc::Status metadata_status = stub->ModelMetadata(&metadata_context, metadata_request, &metadata_response);
            if (!metadata_status.ok()) {
                SetError(error_message, std::string("failed to fetch metadata for model: ") + model_name);
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
                SetError(error_message, std::string("model metadata missing inputs or outputs: ") + model_name);
                return false;
            }

            return true;
        }

        bool ValidateInferResponse(const ModelSpec& spec, const inference::ModelInferResponse& response, int batch_size, std::string* error_message) {
            if (response.outputs_size() != static_cast<int>(spec.outputs.size())) {
                SetError(error_message, std::string("unexpected output count for model: ") + spec.name);
                return false;
            }

            for (int i = 0; i < response.outputs_size(); i++) {
                const auto& output = response.outputs(i);
                if (output.name() != spec.outputs[i].name) {
                    SetError(error_message, std::string("unexpected output tensor name for model: ") + spec.name);
                    return false;
                }

                if (output.shape_size() > 0 && output.shape(0) != batch_size) {
                    SetError(error_message, std::string("unexpected output batch dimension for model: ") + spec.name);
                    return false;
                }
            }

            return true;
        }

        bool RunBatchTrial(
            inference::GRPCInferenceService::Stub* stub,
            const ModelSpec& spec,
            int batch_size,
            RunningStats* latency_stats,
            RunningStats* throughput_stats,
            std::string* error_message) {
            inference::ModelInferRequest request;
            request.set_model_name(spec.name);
            if (!spec.version.empty()) {
                request.set_model_version(spec.version);
            }

            for (size_t i = 0; i < spec.inputs.size(); i++) {
                const TensorSpec& input_spec = spec.inputs[i];
                auto* input = request.add_inputs();
                input->set_name(input_spec.name);
                input->set_datatype(input_spec.datatype);

                std::vector<int64_t> request_shape;
                BuildRequestShape(input_spec.shape, batch_size, &request_shape);
                for (size_t j = 0; j < request_shape.size(); j++) {
                    input->add_shape(request_shape[j]);
                }

                size_t element_count = 1;
                for (size_t j = 0; j < request_shape.size(); j++) {
                    if (request_shape[j] <= 0) {
                        SetError(error_message, std::string("invalid request shape for model: ") + spec.name);
                        return false;
                    }
                    element_count *= static_cast<size_t>(request_shape[j]);
                }

                const size_t bytes_per_element = DatatypeSizeBytes(input_spec.datatype, error_message);
                if (bytes_per_element == 0) {
                    return false;
                }

                std::string raw_contents(element_count * bytes_per_element, '\0');
                request.add_raw_input_contents(raw_contents);
            }

            for (size_t i = 0; i < spec.outputs.size(); i++) {
                auto* output = request.add_outputs();
                output->set_name(spec.outputs[i].name);
            }

            inference::ModelInferResponse response;
            grpc::ClientContext infer_context;

            const auto start_time = std::chrono::steady_clock::now();
            grpc::Status infer_status = stub->ModelInfer(&infer_context, request, &response);
            const auto end_time = std::chrono::steady_clock::now();

            if (!infer_status.ok()) {
                SetError(error_message, std::string("inference failed for model: ") + spec.name);
                return false;
            }

            if (!ValidateInferResponse(spec, response, batch_size, error_message)) {
                return false;
            }

            const double latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            const double throughput = static_cast<double>(batch_size) / (latency_ms / 1000.0);

            StatsPush(*latency_stats, latency_ms);
            StatsPush(*throughput_stats, throughput);

            return true;
        }

        bool WriteReport(const cli::Args& args, const nlohmann::json& report, std::string* error_message) {
            FILE* file = std::fopen(args.output_path, "w");
            if (file == nullptr) {
                SetError(error_message, std::string("failed to open output file: ") + args.output_path);
                return false;
            }

            const std::string serialized = report.dump(4);
            const size_t written = std::fwrite(serialized.data(), 1, serialized.size(), file);
            std::fclose(file);

            if (written != serialized.size()) {
                SetError(error_message, std::string("failed to write output file: ") + args.output_path);
                return false;
            }

            return true;
        }

    } // namespace

    bool RunBenchmark(const cli::Args& args, nlohmann::json* report, std::string* error_message) {
        if (report == nullptr) {
            SetError(error_message, "report output pointer is null");
            return false;
        }

        *report = nlohmann::json::object();

        ServerProcess server = {};
        server.pid = -1;
        bool server_started = false;

        auto cleanup = [&]() {
            if (server_started) {
                StopServer(server);
                server_started = false;
            }
        };

        if (!LaunchServer(args.server_binary_path, &server, error_message)) {
            cleanup();
            return false;
        }
        server_started = true;

        auto channel = grpc::CreateChannel(args.server_address, grpc::InsecureChannelCredentials());
        auto stub = inference::GRPCInferenceService::NewStub(channel);

        if (!WaitForServerReady(stub.get(), server.pid, error_message)) {
            cleanup();
            return false;
        }

        grpc::ClientContext server_metadata_context;
        inference::ServerMetadataRequest server_metadata_request;
        inference::ServerMetadataResponse server_metadata_response;
        grpc::Status server_metadata_status = stub->ServerMetadata(&server_metadata_context, server_metadata_request, &server_metadata_response);
        if (!server_metadata_status.ok()) {
            SetError(error_message, "failed to fetch server metadata");
            cleanup();
            return false;
        }

        std::vector<ModelSpec> models;
        models.reserve(static_cast<size_t>(args.model_count));

        for (int i = 0; i < args.model_count; i++) {
            ModelSpec spec = {};
            if (!LoadModelSpec(stub.get(), args.model_names[i], &spec, error_message)) {
                cleanup();
                return false;
            }
            models.push_back(spec);
        }

        nlohmann::json report_models = nlohmann::json::array();
        nlohmann::json report_summary = nlohmann::json::array();

        for (size_t i = 0; i < models.size(); i++) {
            const ModelSpec& spec = models[i];
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

            for (int batch_index = 0; batch_index < args.batch_size_count; batch_index++) {
                const int batch_size = args.batch_sizes[batch_index];
                RunningStats latency_stats = {};
                RunningStats throughput_stats = {};
                StatsInit(latency_stats);
                StatsInit(throughput_stats);

                for (int trial = 0; trial < args.num_trials; trial++) {
                    if (!RunBatchTrial(stub.get(), spec, batch_size, &latency_stats, &throughput_stats, error_message)) {
                        cleanup();
                        return false;
                    }
                }

                nlohmann::json summary_row = nlohmann::json::object();
                summary_row["model_name"] = spec.name;
                summary_row["batch_size"] = batch_size;
                summary_row["num_trials"] = args.num_trials;
                summary_row["mean_latency_ms"] = latency_stats.mean;
                summary_row["std_latency_ms"] = StatsStdDev(latency_stats);
                summary_row["stderr_latency_ms"] = (latency_stats.count > 0) ? StatsStdDev(latency_stats) / std::sqrt(static_cast<double>(latency_stats.count)) : 0.0;
                summary_row["mean_throughput_infer_per_sec"] = throughput_stats.mean;
                summary_row["std_throughput_infer_per_sec"] = StatsStdDev(throughput_stats);
                summary_row["stderr_throughput_infer_per_sec"] = (throughput_stats.count > 0) ? StatsStdDev(throughput_stats) / std::sqrt(static_cast<double>(throughput_stats.count)) : 0.0;
                report_summary.push_back(summary_row);
            }
        }

        nlohmann::json config_json = nlohmann::json::object();
        config_json["num_trials"] = args.num_trials;
        config_json["server_binary_path"] = args.server_binary_path;
        config_json["server_address"] = args.server_address;
        config_json["output_path"] = args.output_path;

        nlohmann::json batch_sizes_json = nlohmann::json::array();
        for (int i = 0; i < args.batch_size_count; i++) {
            batch_sizes_json.push_back(args.batch_sizes[i]);
        }
        config_json["batch_sizes"] = batch_sizes_json;

        nlohmann::json model_names_json = nlohmann::json::array();
        for (int i = 0; i < args.model_count; i++) {
            model_names_json.push_back(args.model_names[i]);
        }
        config_json["model_names"] = model_names_json;

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

        if (!WriteReport(args, *report, error_message)) {
            cleanup();
            return false;
        }

        cleanup();
        return true;
    }

} // namespace benchmark