# TierAlloc: Tiered Memory Allocator


TierAlloc is a C++ library I'm currently designing to learn more about memory management, especially in large scale ML systems where performance is critical. It is still very much a work in progress. My ultimate goal is to create a lightweight library for efficient memory management across multiple tiers of memory and eventually add support for Linux hugepages and CXL Pooling. It currently provides a flexible allocation API, simulated performance modeling, NUMA support, and integration with PyTorch for optimizing CPU memory allocations.


## Features


- **Tiered Memory Allocation**: Explicitly allocate memory into defined tiers (Fast, Normal, Slow) based on performance characteristics.
- **Allocation Hints**: Use hints like `HOT`, `WARM`, `COLD`, `PIN_FAST`, and `PREFER_FAST` to guide memory placement.
- **Resource Throttling**: Simulate bandwidth and latency costs for each memory tier using a token bucket algorithm, allowing for performance modeling and analysis.
- **Capacity Management**: Configure soft and hard capacity limits for each tier, with policies to handle over-capacity situations (e.g., rerouting allocations to slower tiers or failing them).
- **NUMA Awareness**: Probes NUMA architecture to map memory tiers to specific NUMA nodes, leveraging system-level memory hierarchies when `libnuma` is available.
- **Statistics and Monitoring**: Provides detailed statistics on allocation/deallocation calls, current memory residency, total bytes allocated/freed, simulated wait times, capacity violations, and migration activities, accessible via a JSON output.
- **Memory Interposition**: Optionally interposes standard C library memory allocation functions (`malloc`, `free`, `calloc`, `realloc`) for seamless integration with existing applications.
- **PyTorch Integration**: Offers a CPU allocator shim for PyTorch, enabling PyTorch tensors to be allocated and managed by TierAlloc, with Python bindings to control allocation hints and enable/disable the shim.
- **Memory Migration (Simulated)**: Includes a `ta_move` function to simulate memory migration between tiers by copying data, useful for dynamic memory rebalancing.


## Project Structure


- `CMakeLists.txt`: Main CMake build script for the `tieralloc` library, command-line tool, and benchmarks.
- `include/`: Contains public header files, defining the `tieralloc` API (`tieralloc.h`) and NUMA probing structures (`numa_probe.h`).
- `src/`: Core implementation of the `tieralloc` library:
   - `allocator.cc`: Implements core memory allocation, deallocation, and simulated migration.
   - `policy.cc`: Handles tier selection based on hints and enforces capacity limits.
   - `throttle.cc`: Implements the token bucket algorithm for simulating memory access costs.
   - `stats.cc`: Manages and exposes internal statistics of the allocator.
   - `interpose.cc`: Provides `LD_PRELOAD` functionality to interpose standard C allocation calls.
   - `migrate.cc`: (Placeholder) Intended for actual memory migration implementation.
   - `numa_probe.cc`: Detects and configures NUMA topology for tiered memory mapping.
- `pytorch_shim/`: Contains components for PyTorch integration:
   - `CMakeLists.txt`: CMake build script for the PyTorch shim library.
   - `setup.py`: Python `setuptools` script for building and installing the `tieralloc_shim` Python module.
   - `tieralloc_shim.cpp`: C++ source for the PyTorch CPU allocator shim, integrating `tieralloc` with PyTorch's memory management.
   - `tieralloc_torch.cpp`: (Placeholder) Likely intended for future PyTorch-related functionality.
- `benchmark/`: Contains benchmark programs:
   - `bench_alloc.cc`: Tests basic allocation and deallocation across tiers.
   - `bench_roll.cc`: Simulates a rolling allocation pattern with memory demotion (migration).
- `tools/`: Command-line utilities:
   - `tierallocctl.cc`: A tool to print `tieralloc` statistics in JSON format.


## Building the Project


The project uses CMake for its build system. To build:


```bash
mkdir build
cd build
cmake ..
make
```


## Usage


### TierAlloc Library


The `tieralloc` library can be linked into your C++ applications. Key functions include:


- `ta_alloc(bytes, hint)`: Allocate memory with a specified size and hint.
- `ta_free(p)`: Free allocated memory.
- `ta_move(p, dst_tier)`: Migrate memory from its current tier to a destination tier.
- `ta_get_stats(out)` / `ta_stats_json(buf, n)`: Retrieve allocation statistics.


### Environment Variables


TierAlloc's behavior can be configured using environment variables:


- `TA_INTERPOSE=1`: Enable `LD_PRELOAD` interposition of `malloc`/`free`.
- `TA_DISABLE=1`: Disable `tieralloc` even if interposition is enabled.
- `TA_FAST_SOFT`, `TA_NORMAL_SOFT`, `TA_SLOW_SOFT`: Soft capacity limits for tiers (e.g., `10G`, `512M`, `2K`).
- `TA_FAST_HARD`, `TA_NORMAL_HARD`, `TA_SLOW_HARD`: Hard capacity limits for tiers.
- `TA_ON_HARDCAP=fail`: Change behavior on hard cap violation from default `route_slow` to `fail`.
- `TA_NODE_FAST`, `TA_NODE_NORMAL`, `TA_NODE_SLOW`: Map tiers to specific NUMA node IDs.
- `TA_USE_LIBNUMA=1`: Force usage of `libnuma` if available, otherwise `0` for simulated.


### PyTorch Integration


To use `tieralloc` as the CPU allocator in PyTorch:


```python
import tieralloc_shim


# Enable the tieralloc CPU allocator
tieralloc_shim.enable()


# Set a default hint for subsequent allocations
tieralloc_shim.set_default_hint("hot")


# Check if tieralloc is working
print(tieralloc_shim.hello())
```


### Command-line Tool


The `tierallocctl` tool can be used to inspect the allocator's state:


```bash
./build/tools/tierallocctl stats
```


## Benchmarks


Run the benchmarks from the `build` directory:


```bash
./build/benchmark/bench_alloc
./build/benchmark/bench_roll
```
