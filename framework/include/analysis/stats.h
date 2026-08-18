#pragma once

#include <cstdint>

#include <hdr/hdr_histogram.h>

namespace analysis {

    struct RunningStats {
        struct hdr_histogram* histogram;
        // double sum;
        // double squares_sum;
        // double count;
    };

    bool RunningStatsInit(RunningStats& stats);
    void RunningStatsPush(RunningStats& stats, int64_t value);
    bool RunningStatsDestroy(RunningStats& stats);
    bool RunningStatsClear(RunningStats& stats);

    bool RunningStatsAdd(RunningStats& dest, const RunningStats& src);

    double RunningStatsMean(RunningStats stats);
    double RunningStatsStdDev(RunningStats stats);
    double RunningStatsStdErr(RunningStats stats);
    double RunningStatsPercentile(RunningStats stats, double p);

} // namespace analysis
