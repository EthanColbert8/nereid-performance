#pragma once

#include "cli/args.h"
#include "logging/logger.h"
#include "nereid/model.h"

#include <string>
#include <sys/types.h>

namespace benchmark {

    struct BenchmarkContext {
        const char* server_address; // already put together with port
        const nereid::ModelSpec* model_specs;
        int model_count;
        const int* batch_sizes;
        int batch_size_count;
        int num_trials;
    };

    bool BuildBenchmarkContext(const cli::Args& args, BenchmarkContext* context, logging::Logger& logger);

} // namespace benchmark
