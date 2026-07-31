#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "cli/args.h"

namespace benchmark {

    bool RunBenchmark(const cli::Args& args, nlohmann::json* report, std::string* error_message);

} // namespace benchmark