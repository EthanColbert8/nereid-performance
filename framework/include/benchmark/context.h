#pragma once

#include "config/settings.h"
#include "logging/logger.h"
#include "nereid/model.h"

#include <sys/types.h>
#include <string>
#include <vector>

namespace benchmark {

    enum class StopConditionType {
        TIME,
        NUM_TRIALS,
        SIGNAL
    };

    struct Step {
        nereid::ModelSpec model_spec;
        int batch_size;

        int stop_value;
        StopConditionType stop_condition;
    };

    struct Sequence {
        std::vector<Step> steps;
    };

    struct Stage {
        std::vector<Sequence> client_sequences;
        // Need something to know what signals to send and when
    };

    struct Run {
        std::string name;
        std::vector<Stage> stages;
    };

    struct BenchmarkContext {
        // int num_trials;
        std::string server_address; // already put together with port
        std::vector<nereid::ModelSpec> model_specs;
        // std::vector<int> batch_sizes;
        std::vector<Run> runs;
    };

    // TODO: fix the implementation here to build runs properly
    bool BuildBenchmarkContext(const config::Settings& args, BenchmarkContext& context, logging::Logger& logger);

} // namespace benchmark
