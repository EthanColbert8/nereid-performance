# Todo list for the benchmarking framework
1. [x] Refactor so that tight loop over trials is fast
2. [x] Make sizes of all dynamic dimensions configurable
3. [x] Generate random numbers in buffers instead of zeros
4. [x] Pre-allocate buffers for each batch size
5. [ ] Allow for connecting to external server (no launching - separate process management from runner for this)
6. [x] Create a context struct for the runner
7. [ ] Create a logging system
8. [ ] Create an actual enum for data types, rather than using a string