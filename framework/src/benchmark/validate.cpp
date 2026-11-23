#include "benchmark/validate.h"
#include "benchmark/context.h"
#include "benchmark/desc.h"
#include "logging/logger.h"

#include <cstddef>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace benchmark {

    bool ValidateRunDefinitions(const std::vector<Run>& runs, logging::Logger& logger) {
        bool ok = true;

        if (runs.empty()) {
            logger.error("no benchmark runs were defined");
            return false;
        }

        const unsigned int hardware_threads = std::thread::hardware_concurrency(); // 0 if unknown
        std::unordered_set<std::string> run_names;

        for (const Run& run : runs) {
            const char* run_name = run.name.c_str();

            if (!run_names.insert(run.name).second) {
                logger.error("run name \"%s\" is used more than once (run names must be unique)", run_name);
                ok = false;
            }

            if (run.stages.empty()) {
                logger.error("run \"%s\" has no stages", run_name);
                ok = false;
            }

            for (size_t stage_idx = 0; stage_idx < run.stages.size(); stage_idx++) {
                const Stage& stage = run.stages[stage_idx];

                if (stage.client_sequences.empty()) {
                    logger.error("run \"%s\", stage %zu has no client sequences", run_name, stage_idx);
                    ok = false;
                }

                if (hardware_threads > 0 && stage.client_sequences.size() > hardware_threads) {
                    logger.warning(
                        "run \"%s\", stage %zu has %zu clients but only %u hardware threads are available; "
                        "the benchmark client may become the bottleneck instead of the server",
                        run_name, stage_idx, stage.client_sequences.size(), hardware_threads
                    );
                }

                for (size_t client_idx = 0; client_idx < stage.client_sequences.size(); client_idx++) {
                    const Sequence& sequence = stage.client_sequences[client_idx];

                    if (sequence.steps.empty()) {
                        logger.error("run \"%s\", stage %zu, client %zu has no steps", run_name, stage_idx, client_idx);
                        ok = false;
                    }

                    for (size_t step_idx = 0; step_idx < sequence.steps.size(); step_idx++) {
                        const Step& step = sequence.steps[step_idx];

                        if (step.stop_condition == StopConditionType::SIGNAL) {
                            logger.error(
                                "run \"%s\", stage %zu, client %zu, step %zu: the SIGNAL stop condition is not yet implemented",
                                run_name, stage_idx, client_idx, step_idx
                            );
                            ok = false;
                        }
                        else if (step.stop_value <= 0) {
                            logger.error(
                                "run \"%s\", stage %zu, client %zu, step %zu: stop_value must be positive (got %d)",
                                run_name, stage_idx, client_idx, step_idx, step.stop_value
                            );
                            ok = false;
                        }

                        if (step.batch_size <= 0) {
                            logger.error(
                                "run \"%s\", stage %zu, client %zu, step %zu: batch_size must be positive (got %d)",
                                run_name, stage_idx, client_idx, step_idx, step.batch_size
                            );
                            ok = false;
                        }
                    }
                }
            }
        }

        return ok;
    }

    bool ValidateRunModels(const BenchmarkContext& ctx, logging::Logger& logger) {
        bool ok = true;

        for (const Run& run : ctx.runs) {
            for (size_t stage_idx = 0; stage_idx < run.stages.size(); stage_idx++) {
                const Stage& stage = run.stages[stage_idx];

                for (size_t client_idx = 0; client_idx < stage.client_sequences.size(); client_idx++) {
                    const Sequence& sequence = stage.client_sequences[client_idx];

                    for (size_t step_idx = 0; step_idx < sequence.steps.size(); step_idx++) {
                        const Step& step = sequence.steps[step_idx];

                        if (ctx.model_directory.find(step.model_name) == ctx.model_directory.end()) {
                            logger.error(
                                "run \"%s\", stage %zu, client %zu, step %zu: model \"%s\" is not configured or was not found on the server",
                                run.name.c_str(), stage_idx, client_idx, step_idx, step.model_name.c_str()
                            );
                            ok = false;
                        }
                    }
                }
            }
        }

        return ok;
    }

} // namespace benchmark
