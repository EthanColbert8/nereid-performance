#pragma once


namespace analysis {

    struct RunningStats {
        double sum;
        double squares_sum;
        double count;
    };

    void RunningStatsInit(RunningStats& stats);
    void RunningStatsPush(RunningStats& stats, double value);

    double RunningStatsMean(RunningStats stats);
    double RunningStatsStdDev(RunningStats stats);
    double RunningStatsStdErr(RunningStats stats);

} // namespace analysis
