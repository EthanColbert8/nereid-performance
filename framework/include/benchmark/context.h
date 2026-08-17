#pragma once

#include "config/settings.h"
#include "logging/logger.h"
#include "nereid/model.h"

#include <string>
#include <sys/types.h>

namespace benchmark {

    // TODO (Ethan): change this guy to just use vectors, it's too much work to try to translate all the time
    struct BenchmarkContext {
        const char* server_address; // already put together with port
        const nereid::ModelSpec* model_specs;
        int model_count;
        const int* batch_sizes;
        int batch_size_count;
        int num_trials;
    };

    bool BuildBenchmarkContext(const config::Settings& args, BenchmarkContext* context, logging::Logger& logger);

} // namespace benchmark
