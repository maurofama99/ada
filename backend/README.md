# ADA: Adaptive Windowing for Continuous Regular Path Queries over Streaming Graphs

## Build

Prerequisites: C++17 compiler (`g++` or `clang++`), sources under `source/`.

Example (macOS / Linux):

```bash
g++ -O3 -std=c++17 -I./source main.cpp source/*.cpp -o streaming_rpq
```

## Run

The program accepts a single argument: the path to the configuration file.

```bash
./streaming_rpq config.txt
```

## Input graph file format

The file referenced by `input_data_path` must contain one edge per line:

```
s d l t
```

Where:
- `s`: source node (long long)
- `d`: destination node
- `l`: integer label
- `t`: timestamp (monotonic non\-necessarily contiguous)

Only edges whose label appears in `labels` are processed.

## Configuration file

Key\=value format, one pair per line, no spaces. Example:

```ini
input_data_path=data/sample_edges.txt
adaptive=1
size=1000
slide=200
max_size=4000
min_size=400
query_type=5
labels=10,11,12
```

### Parameters

| Key | Description |
|-----|-------------|
| `input_data_path` | Relative or absolute path to the edge list. Resolved against the executable directory. |
| `adaptive` | `0` = fixed-size sliding window, `1` = adaptive window. |
| `size` | Initial time window width (timestamp units). |
| `slide` | Slide step (if equal to size rollbacks to tumbling window). |
| `max_size` | Upper bound for adaptive window size. |
| `min_size` | Lower bound for adaptive window size. |
| `query_type` | Query identifier (builds one of the prepared rpqs automaton, see below). |
| `labels` | Comma separated integers: ordered symbols used in automaton transitions. |

### Supported queries (`query_type`)

| `query_type` | Pattern |
|--------------|---------|
| 1 | `a+` | 
| 2 | `ab*` | 
| 3 | `ab*c*` | 
| 4 | `(abc)+` | 
| 5 | `ab*c` | 
| 6 | `a*b*` |
| 7 | `abc*` | 
| 10 | `(a\|b)c*` | 

Ensure the number of provided labels matches the distinct symbols required.

## Generated CSV outputs

Prefix: dataset folder name (`data_folder`) followed by type and parameters.

1. `_summary_results_...csv`  
   Columns:  
   - `total_edges`: processed edges (filtered by allowed labels)  
   - `matches`: total recognized paths  
   - `exec_time`: total execution time (seconds)  
   - `windows_created`: number of windows generated  
   - `avg_window_cardinality`: average edges per window  
   - `avg_window_size`: average effective time span (adaptive)  

2. `_window_results_...csv`  
   - `index`  
   - `t_open`  
   - `t_close`  
   - `window_results`: results emitted at window close  
   - `incremental_matches`: cumulative matches up to this window  
   - `latency`: window completion time  
   - `window_cardinality`  
   - `window_size`  

3. `_tuples_results_...csv`  
   - `estimated_cost`  
   - `normalized_estimated_cost`  
   - `latency`  
   - `normalized_latency`  
   - `window_cardinality`  
   - `widow_size` (note: original field name contains a typo)  

4. `_memory_results_...csv` (only if `MEMORY_PROFILER` set to `true` at compile time)  
   - `tot_virtual`  
   - `used_virtual`  
   - `tot_ram`  
   - `used_ram`  
   - `data_mem` (sum of main data structures)  


## Notes

- No comments supported in the config file.
- Do not insert spaces around `=`.
- Timestamps should be non\-decreasing for predictable behavior.
- Memory profiling code is supported only for Linux os, disabled by default (`MEMORY_PROFILER false` in `main.cpp`).


