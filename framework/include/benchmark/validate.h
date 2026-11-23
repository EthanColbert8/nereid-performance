#pragma once

#include "benchmark/context.h"
#include "benchmark/desc.h"
#include "logging/logger.h"

#include <vector>

namespace benchmark {

    // Checks the structure of the run definitions before anything is launched: every run, stage and
    // client sequence is non-empty, run names are unique, and every step has a positive batch size and
    // stop value and a supported stop condition. Logs every problem found, then returns false if any.
    // Also warns (without failing) when a stage has more clients than available hardware threads.
    bool ValidateRunDefinitions(const std::vector<Run>& runs, logging::Logger& logger);

    // Checks that every step's model is in the context's model directory (models that don't exist
    // on the server are skipped when the context is built). Logs every problem found.
    bool ValidateRunModels(const BenchmarkContext& ctx, logging::Logger& logger);

} // namespace benchmark
