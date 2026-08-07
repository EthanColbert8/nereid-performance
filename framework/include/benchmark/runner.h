#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "benchmark/context.h"

namespace benchmark {

    bool RunBenchmark(const BenchmarkContext& ctx, nlohmann::json* report, std::string* error_message);

} // namespace benchmark