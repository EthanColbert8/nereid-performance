#pragma once

#include "benchmark/desc.h"
#include "analysis/stats.h"
#include "logging/logger.h"
#include "nereid/model.h"

#include <chrono>
#include <functional>
#include <random>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace benchmark {

    // Called after every successful request with the time the response was received.
    // Return false to end the loop early (e.g. on a received signal).
    using RequestLoopHook = std::function<bool(std::chrono::steady_clock::time_point now)>;

    // Sends inference requests for one step until its stop condition is met or the hook
    // asks to stop. Latencies are recorded into latency_stats in microseconds.
    //   NUM_TRIALS: stop after step.stop_value requests
    //   TIME:       stop after step.stop_value seconds
    //   SIGNAL:     run until the hook returns false (hook is required)
    // Returns false only on failure (RPC error, invalid response, bad step).
    bool RunRequestLoop(
        inference::GRPCInferenceService::Stub* stub,
        const nereid::ModelSpec& spec,
        const Step& step,
        std::mt19937& rng,
        analysis::RunningStats& latency_stats,
        logging::Logger* logger,
        const RequestLoopHook& hook = nullptr
    );

} // namespace benchmark
