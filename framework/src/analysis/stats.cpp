#include "analysis/stats.h"

#include <cstdint>
#include <cmath>

#include <hdr/hdr_histogram.h>

namespace analysis {

    bool RunningStatsInit(RunningStats& stats) {
        // stats.sum = 0.0;
        // stats.squares_sum = 0.0;
        // stats.count = 0.0;

        // hard-coded values appropriate for our latency stuff
        int rc = hdr_init(1, 60000000, 3, &stats.histogram);
        return rc == 0;
    }

    void RunningStatsPush(RunningStats& stats, int64_t value) {
        // double dv = static_cast<double>(value);
        // stats.sum += dv;
        // stats.squares_sum += dv * dv;
        // stats.count += 1.0;

        hdr_record_value(stats.histogram, value);
    }

    bool RunningStatsDestroy(RunningStats& stats) {
        hdr_close(stats.histogram);
        stats.histogram = nullptr;
        return true;
    }

    bool RunningStatsClear(RunningStats& stats) {
        hdr_reset(stats.histogram);
        return true;
    }

    bool RunningStatsAdd(RunningStats& dest, const RunningStats& src) {
        int rc = hdr_add(dest.histogram, src.histogram);
        return rc == 0;
    }

    double RunningStatsMean(RunningStats stats) {
        // if (stats.count < 1.0) {
        //     return 0.0;
        // }
        // return stats.sum / stats.count;

        return hdr_mean(stats.histogram);
    }

    double RunningStatsStdDev(RunningStats stats) {
        // if (stats.count < 2.0) {
        //     return 0.0;
        // }

        // double variance = (stats.squares_sum - (stats.sum * stats.sum / stats.count)) / (stats.count - 1.0);
        // return std::sqrt(variance);

        return hdr_stddev(stats.histogram);
    }

    double RunningStatsStdErr(RunningStats stats) {
        // if (stats.count < 2.0) {
        //     return 0.0;
        // }
        // return RunningStatsStdDev(stats) / std::sqrt(stats.count);

        int64_t count = stats.histogram->total_count;
        if (count < 2) {
            return 0.0;
        }
        return hdr_stddev(stats.histogram) / std::sqrt(static_cast<double>(count));
    }

    double RunningStatsPercentile(RunningStats stats, double p) {
        return static_cast<double>(hdr_value_at_percentile(stats.histogram, p));
    }

} // namespace analysis
