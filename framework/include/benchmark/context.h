#pragma once

#include "benchmark/desc.h"
#include "config/settings.h"
#include "logging/logger.h"
#include "nereid/model.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace benchmark {

    struct BenchmarkContext {
        // int num_trials;
        std::string server_address; // already put together with port
        std::unordered_map<std::string, nereid::ModelSpec> model_directory; // maps name->spec
        // std::vector<int> batch_sizes;
        std::vector<Run> runs;
    };

    bool BuildBenchmarkContext(const config::Settings& args, BenchmarkContext& context, logging::Logger& logger);

} // namespace benchmark
