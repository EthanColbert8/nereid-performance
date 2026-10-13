#include "benchmark/requests.h"
#include "benchmark/desc.h"
#include "analysis/stats.h"
#include "logging/logger.h"
#include "nereid/model.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace benchmark {

    namespace {

        // NOTE (Ethan): duplicates helpers in runner.cpp until BenchmarkRunner moves over to RunRequestLoop
        bool ValidateResponse(
            const nereid::ModelSpec& spec,
            const inference::ModelInferResponse& response,
            int batch_size,
            logging::Logger* logger
        ) {
            if (response.outputs_size() != static_cast<int>(spec.outputs.size())) {
                logger->error("unexpected output count for model \"%s\": expected %zu, got %d", spec.name.c_str(), spec.outputs.size(), response.outputs_size());
                return false;
            }

            for (int i = 0; i < response.outputs_size(); i++) {
                const auto& output = response.outputs(i);
                if (output.name() != spec.outputs[i].name) {
                    logger->error(
                        "unexpected output tensor name for model \"%s\": expected \"%s\", got \"%s\"",
                        spec.name.c_str(), spec.outputs[i].name.c_str(), output.name().c_str()
                    );
                    return false;
                }

                if (output.shape_size() > 0 && output.shape(0) != batch_size) {
                    logger->error("unexpected output batch dimension for model \"%s\": expected %d, got %lld", spec.name.c_str(), batch_size, static_cast<long long>(output.shape(0)));
                    return false;
                }
            }

            return true;
        }

    } // namespace

    bool RunRequestLoop(
        inference::GRPCInferenceService::Stub* stub,
        const nereid::ModelSpec& spec,
        const Step& step,
        std::mt19937& rng,
        analysis::RunningStats& latency_stats,
        logging::Logger* logger,
        const RequestLoopHook& hook
    ) {
        if (step.stop_value <= 0 && step.stop_condition != StopConditionType::SIGNAL) {
            logger->error("invalid stop value %d for model \"%s\"", step.stop_value, spec.name.c_str());
            return false;
        }
        if (step.stop_condition == StopConditionType::SIGNAL && !hook) {
            logger->error("SIGNAL stop condition requires a hook to end the request loop");
            return false;
        }

        const int batch_size = step.batch_size;

        inference::ModelInferRequest request;
        request.set_model_name(spec.name);
        if (!spec.version.empty()) {
            request.set_model_version(spec.version);
        }

        // Pre-allocating all the input buffers
        std::vector<std::vector<float>> input_buffers;
        input_buffers.reserve(spec.inputs.size());

        for (size_t i = 0; i < spec.inputs.size(); i++) {
            const nereid::TensorSpec& input_spec = spec.inputs[i];

            auto* input = request.add_inputs();
            input->set_name(input_spec.name);
            input->set_datatype(nereid::DtypeToString(input_spec.dtype));

            size_t element_count = batch_size;
            input->add_shape(batch_size);
            for (size_t j = 0; j < input_spec.shape.size(); j++) {
                input->add_shape(input_spec.shape[j]);
                element_count *= static_cast<size_t>(input_spec.shape[j]);
            }

            input_buffers.emplace_back(element_count);
        }

        for (size_t i = 0; i < spec.outputs.size(); i++) {
            auto* output = request.add_outputs();
            output->set_name(spec.outputs[i].name);
        }

        inference::ModelInferResponse response;
        std::normal_distribution<float> dist(0.0f, 1.0f);

        const auto loop_start = std::chrono::steady_clock::now();
        const auto deadline = loop_start + std::chrono::seconds(step.stop_value);

        // the tight loop around running inference trials
        for (int64_t trial = 0; ; trial++) {
            if (step.stop_condition == StopConditionType::NUM_TRIALS && trial >= step.stop_value) { break; }

            request.clear_raw_input_contents();

            // ClientContext is single-use for some reason...
            grpc::ClientContext infer_context;

            for (auto& buffer : input_buffers) {
                for (float& value : buffer) { value = dist(rng); }
            }

            const auto start_time = std::chrono::steady_clock::now();

            // Include input preparation for RPC call in the timing
            for (const auto& buffer : input_buffers) {
                request.add_raw_input_contents(buffer.data(), buffer.size() * sizeof(float));
            }

            grpc::Status infer_status = stub->ModelInfer(&infer_context, request, &response);

            const auto end_time = std::chrono::steady_clock::now();

            if (!infer_status.ok()) {
                logger->error(
                    "inference failed for model \"%s\". Error code: %d, message: %s",
                    spec.name.c_str(), infer_status.error_code(), infer_status.error_message().c_str()
                );
                return false;
            }

            if (!ValidateResponse(spec, response, batch_size, logger)) { return false; }

            const int64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            analysis::RunningStatsPush(latency_stats, latency_us);

            if (hook && !hook(end_time)) { break; }
            if (step.stop_condition == StopConditionType::TIME && end_time >= deadline) { break; }
        }

        return true;
    }

} // namespace benchmark
