#pragma once

#include "benchmark/context.h"
#include "logging/logger.h"

#include <string>

#include <nlohmann/json.hpp>

namespace benchmark {

    bool RunSingleClientBenchmark(const BenchmarkContext& ctx, nlohmann::json* report, logging::Logger& logger);

} // namespace benchmark