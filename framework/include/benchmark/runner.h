#pragma once

#include "benchmark/context.h"
#include "logging/logger.h"

#include <string>
#include <random>

#include <nlohmann/json.hpp>
#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace benchmark {

    struct standard_normal_generator {
        std::mt19937* generator;
        std::normal_distribution<float> dist;
    };


    class BenchmarkRunner {
        private:
            logging::Logger* logger;
            BenchmarkContext ctx;
            standard_normal_generator rand_gen;
            nlohmann::json* report;

            bool ValidateInferResponse(const nereid::ModelSpec& spec, const inference::ModelInferResponse& response, int batch_size);
            bool RunBatchTrials(inference::GRPCInferenceService::Stub* stub, const Step& step, analysis::RunningStats& latency_stats, analysis::RunningStats& throughput_stats);

        public:
            BenchmarkRunner(logging::Logger* logger, const BenchmarkContext& ctx, nlohmann::json* report);

            bool RunSingleClient(size_t run_idx);
    };

} // namespace benchmark