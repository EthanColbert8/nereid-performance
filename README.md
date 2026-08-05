# Nereid performance analysis

This repository is just some collected utilities for measuring the performance of the [Nereid inference server](https://github.com/ngpaladi/nereid-server). Targeted towards running benchmarks on HPC systems.

## Framework benchmark

The C++ benchmark in `framework/` launches the configured Nereid server binary, waits for it to become ready, fetches server and model metadata, runs a batch-size sweep for each configured model, and writes a summary-only JSON report.

Example:

```bash
cd framework
./build/nereid-bench \
	--server-binary /full/path/to/nereid-server \
	--server-address localhost:50051 \
	--output nereid_benchmark_summary.json
```

The default model list and batch-size sweep live in `framework/src/cli/args.cpp` for now so they are easy to swap to config-driven inputs later. The JSON report includes server metadata, benchmark configuration, per-model metadata, and per-model/per-batch summary statistics, but no raw per-trial latencies.
