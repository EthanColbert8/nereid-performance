# Todo list for the benchmarking framework
[x] Refactor so that tight loop over trials is fast
[x] Make sizes of all dynamic dimensions configurable
[x] Generate random numbers in buffers instead of zeros
[x] Pre-allocate buffers for each batch size
[x] Allow for connecting to external server (no launching - separate process management from runner for this)
[x] Create a context struct for the runner
[x] Create an actual enum for data types, rather than using a string
[x] Create a logging system
[x] Reorganize JSON output - it's awful rn
[ ] Include timestamps of when trials started in benchmark report
[ ] Get Nereid server logs sent to a file (currently it's `/dev/null`) when we own process
[ ] Allow logging to files other than stderr
[ ] Start thinking about benchmark configuration via YAML
[ ] Default to running all loaded models with non-ambiguous input shapes
[ ] Gather GPU hardware information - GPU model, preferably live metrics...
