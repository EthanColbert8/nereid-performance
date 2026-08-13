#pragma once

#include "benchmark/context.h"

#include <string>

#include <nlohmann/json.hpp>

namespace benchmark {

    bool RunBenchmark(const BenchmarkContext& ctx, nlohmann::json* report, std::string* error_message);

} // namespace benchmark