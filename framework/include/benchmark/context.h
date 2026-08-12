#pragma once

#include <string>
#include <sys/types.h>
#include "cli/args.h"
#include "nereid/model.h"

namespace benchmark {

    struct BenchmarkContext {
        const char* server_address; // already put together with port
        const nereid::ModelSpec* model_specs;
        int model_count;
        const int* batch_sizes;
        int batch_size_count;
        int num_trials;
    };

    bool BuildBenchmarkContext(const cli::Args& args, BenchmarkContext* context, std::string* error_message);

} // namespace benchmark
