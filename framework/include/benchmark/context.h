#pragma once

#include "config/settings.h"
#include "logging/logger.h"
#include "nereid/model.h"

#include <sys/types.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

namespace benchmark {

    enum class StopConditionType {
        TIME,
        NUM_TRIALS,
        SIGNAL
    };

    struct Step {
        std::string model_name;
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
        std::unordered_map<std::string, nereid::ModelSpec> model_directory; // maps name->spec
        // std::vector<int> batch_sizes;
        std::vector<Run> runs;
    };

    // TODO: fix the implementation here to build runs properly
    bool BuildBenchmarkContext(const config::Settings& args, BenchmarkContext& context, logging::Logger& logger);

    // added to show the YAML parser how to make a StopConditionType
    inline StopConditionType StringToStopConditionType_throws(const std::string& stop_cond) {
        if (stop_cond == "TIME") { return StopConditionType::TIME; }
        else if (stop_cond == "NUM_TRIALS") { return StopConditionType::NUM_TRIALS; }
        else if (stop_cond == "SIGNAL") { return StopConditionType::SIGNAL; }

        throw std::runtime_error("invalid stop condition type: " + stop_cond);
    }

} // namespace benchmark
