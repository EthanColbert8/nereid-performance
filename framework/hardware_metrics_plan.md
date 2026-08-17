## Plan: NVML-First Hardware Metrics Split Report

Refocus the hardware metrics subsystem on process-scoped NVML sampling for NVIDIA GPUs, with a backend abstraction that allows a future ROCm backend to plug in with minimal API changes. If NVML is unavailable or unsupported, continue collecting CPU/RAM-only metrics without failing the benchmark. Emit hardware metrics to a dedicated JSON artifact instead of embedding them in the benchmark summary JSON.

**Steps**
1. Phase 1 - Metrics model and backend abstraction (*foundational*)
1. Define a backend-neutral GPU metrics interface with capabilities for: per-process compute utilization (best-effort/approximate), per-process GPU memory usage, and static device metadata.
2. Add explicit capability flags and reason codes so the sampler records whether each metric was collected, unavailable, unsupported, or permission-blocked.
3. Keep timestamp model as dual clock (`steady_clock` for monotonic elapsed and `system_clock` for wall correlation).
4. Define a backend enum and selection policy with v1 values: `nvml` and `none`; keep interface shape compatible with adding `rocm_smi` later.

2. Phase 2 - NVML backend and CPU-only fallback (*depends on Phase 1*)
1. Implement `NVMLProcessSampler` with PID-targeted queries for process memory and process utilization where available.
2. Add optional static GPU metadata collection via NVML: device model/name, driver version, and CUDA driver/runtime compatibility version if exposed.
3. Normalize units in runtime model: utilization as percent, memory in bytes, and metadata as strings/integers with explicit unknown/null handling.
4. Implement graceful fallback path when NVML init/library/symbol queries fail: continue with CPU/RAM metrics only and annotate runtime degradation notes.

3. Phase 3 - Sampling engine and threading ownership (*depends on Phases 1-2*)
1. Keep thread lifecycle fully in main: construct stop flag, spawn sampler worker, stop/join on all exits.
2. Keep scraper API blocking and return-by-value to avoid shared mutable metric storage across threads.
3. Use configurable cadence with a default of 100-250 ms and monotonic elapsed timestamps.
4. Capture per-sample freshness/validity markers so short benchmark windows (~1 s) can distinguish stale vs newly observed GPU samples.

4. Phase 4 - CLI/config and runtime integration (*depends on Phase 3*)
1. Add metrics configuration flags: enable/disable hardware metrics, sample interval, and optional explicit server PID for connect mode.
2. In launch mode, auto-bind PID from launched server process; in connect mode, use provided PID when available and otherwise run CPU/RAM-only plus optional device-level metadata.
3. Add separate hardware metrics output path argument (for example `--hardware-output`) with deterministic default filename.
4. Ensure benchmark summary flow remains unchanged except for storing a reference/path to the hardware report only if desired.

5. Phase 5 - Dedicated hardware JSON report (*depends on Phase 4*)
1. Create a standalone hardware metrics JSON schema and writer path independent of benchmark summary serialization.
2. Include report sections: config, runtime backend/capabilities/degradation notes, device metadata, and raw time-series samples.
3. Ensure schema can represent mixed availability per metric (for example memory available but process compute util unavailable).
4. Keep benchmark summary JSON untouched (no embedded hardware sample arrays).

6. Phase 6 - Build/dependency strategy for portability (*parallel with Phase 4 where possible; finalized before verification*)
1. Keep baseline build working without NVML present.
2. Prefer runtime dynamic loading of NVML (`libnvidia-ml.so.1`) to reduce compile/link friction across systems.
3. Gate NVML-backed features on runtime probe success; no hard failure for missing library/permissions.
4. Keep backend interface and CMake structure ready for adding ROCm SMI backend later without changing caller contracts.

7. Phase 7 - Verification and hardening (*depends on Phases 4-6*)
1. Validate behavior on NVIDIA host with NVML: process memory and compute utilization sampled at target cadence.
2. Validate behavior on host without NVML (or forced-disabled): CPU/RAM metrics still collected and hardware report explicitly records GPU metric unavailability.
3. Validate metadata capture fields: GPU model, driver version, CUDA compatibility/version when available; verify explicit unknown values when not.
4. Validate stop/join behavior on success and all early-return failure paths in main.
5. Validate dedicated hardware report emission and no regression to benchmark summary JSON layout.

**Relevant files**
- `/home/emcol/Projects/nereid-performance/framework/include/benchmark/hardware.h` — worker API shape, context fields, and hardware metrics domain model.
- `/home/emcol/Projects/nereid-performance/framework/src/benchmark/hardware.cpp` — sampler loop, NVML backend integration, fallback behavior, and capability annotations.
- `/home/emcol/Projects/nereid-performance/framework/src/main.cpp` — explicit scraper thread lifecycle and separate report write flow.
- `/home/emcol/Projects/nereid-performance/framework/include/cli/args.h` — CLI fields for metrics enablement, cadence, PID override, and hardware report path.
- `/home/emcol/Projects/nereid-performance/framework/src/cli/args.cpp` — argument parsing, validation, and defaults for new hardware flags.
- `/home/emcol/Projects/nereid-performance/framework/CMakeLists.txt` — optional NVML handling and runtime-loading support flags.
- `/home/emcol/Projects/nereid-performance/framework/include/benchmark/` — optional new backend abstraction header(s) for future ROCm parity.
- `/home/emcol/Projects/nereid-performance/framework/src/benchmark/` — optional backend module split (`nvml_backend`, `null_backend`) to keep add-on ROCm backend isolated.

**Verification**
1. Build the framework target on a system with NVIDIA driver/NVML installed and confirm successful compile/link/runtime initialization.
2. Run benchmark in launch mode and verify hardware report includes PID-targeted GPU memory and compute-util samples plus CPU/RAM samples.
3. Run benchmark in connect mode with explicit PID and verify identical process-scoped behavior.
4. Simulate missing NVML (library absent or disabled) and verify benchmark still completes with CPU/RAM-only hardware report and clear degradation notes.
5. Validate device metadata fields in hardware report: model/name, driver version, CUDA version compatibility when exposed; otherwise explicit unknown/null.
6. Confirm benchmark summary JSON remains unchanged and hardware samples only appear in dedicated hardware report file.

**Decisions**
- GPU backend focus for v1: NVML only; remove DCGM from immediate implementation scope.
- Portability strategy: backend abstraction compatible with future ROCm SMI integration.
- Failure policy: missing/unusable NVML never fails benchmark; fallback is CPU/RAM-only metrics.
- Reporting strategy: hardware metrics written to separate JSON artifact, not embedded in benchmark summary.
- Threading ownership: all thread management remains in main for lifecycle visibility.

**Further Considerations**
1. Process compute-util semantics differ by driver/GPU generation; define reporting language as "approximate sampled process utilization" and include capability flags.
2. Decide whether connect mode without PID should allow device-level GPU metrics or enforce strict CPU-only mode for attribution correctness.
3. Consider adding optional "strict metrics" CLI later that fails run when required process GPU metrics are unavailable.
