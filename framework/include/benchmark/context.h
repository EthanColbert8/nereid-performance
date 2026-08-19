#pragma once

#include "config/settings.h"
#include "logging/logger.h"
#include "nereid/model.h"

#include <sys/types.h>
#include <string>
#include <vector>

namespace benchmark {

    struct BenchmarkContext {
        int num_trials;
        std::string server_address; // already put together with port
        std::vector<nereid::ModelSpec> model_specs;
        std::vector<int> batch_sizes;
    };

    bool BuildBenchmarkContext(const config::Settings& args, BenchmarkContext& context, logging::Logger& logger);

} // namespace benchmark
