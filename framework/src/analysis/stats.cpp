#include <analysis/stats.h>

#include <cmath>

namespace analysis {

    void RunningStatsInit(RunningStats& stats) {
        stats.sum = 0.0;
        stats.squares_sum = 0.0;
        stats.count = 0.0;
    }

    void RunningStatsPush(RunningStats& stats, double value) {
        stats.sum += value;
        stats.squares_sum += value * value;
        stats.count += 1.0;
    }

    double RunningStatsMean(RunningStats stats) {
        if (stats.count < 1.0) {
            return 0.0;
        }
        return stats.sum / stats.count;
    }

    double RunningStatsStdDev(RunningStats stats) {
        if (stats.count < 2.0) {
            return 0.0;
        }

        double variance = (stats.squares_sum - (stats.sum * stats.sum / stats.count)) / (stats.count - 1.0);
        return std::sqrt(variance);
    }

    double RunningStatsStdErr(RunningStats stats) {
        if (stats.count < 2.0) {
            return 0.0;
        }
        return RunningStatsStdDev(stats) / std::sqrt(stats.count);
    }

} // namespace analysis
