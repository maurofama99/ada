# Robust Regular Path Queries over Streaming Graphs via Load-Aware Windowing

## Additional materials

### Appendix
The appendix of the paper submitted to VLDB '26 can be found in this file: [Appendix](appendix.pdf)

### Extensive Experiments
We report the experiments of latency, throughput and completeness for all the mentioned Regular Path Queries and for the additional dataset [StackOverflow](https://snap.stanford.edu/data/sx-stackoverflow.html). 

#### StackOverflow
##### Throughput
<img width="4480" height="3200" alt="Throughput_so_tuples_full" src="https://github.com/user-attachments/assets/6f85f09c-d6cb-4a71-80ab-149bc3735963" />

##### Latency
<img width="4480" height="3200" alt="Latency_so_tuples_full" src="https://github.com/user-attachments/assets/8b3456da-2992-4ffa-9d24-0f8ca63b77b7" />

#### LDBC
##### Throughput
<img width="4480" height="2400" alt="Throughput_ldbc_tuples_full" src="https://github.com/user-attachments/assets/9a875845-7632-436c-8873-718803050e36" />

##### Latency
<img width="4480" height="2400" alt="Latency_ldbc_tuples_full" src="https://github.com/user-attachments/assets/7751d982-7898-419f-b40e-650beb421571" />

#### Twitter-Higgs
##### Throughput
<img width="4480" height="4000" alt="Throughput_higgs_tuples_full" src="https://github.com/user-attachments/assets/bcf8c3a7-401a-4a25-8475-40dea562626b" />

##### Latency
<img width="4480" height="4000" alt="Latency_higgs_tuples_full" src="https://github.com/user-attachments/assets/feb2f3d1-f50b-4dc5-aea5-f400b152d902" />


## Build

Prerequisites:  C++17 compiler (`g++` or `clang++`), sources under `source/`.

Example (macOS / Linux):

```bash
g++ -O3 -std=c++17 -I./source main.cpp source/*. cpp -o streaming_rpq
```

## Run

The program accepts a single argument:  the path to the configuration file.

```bash
./streaming_rpq code/config.txt
```

## Input Graph File Format

The file referenced by `input_data_path` must contain one edge per line:

```
s d l t
```

Where:
- `s`: source node (long long)
- `d`: destination node
- `l`: integer label
- `t`: timestamp (monotonic, non-necessarily contiguous)

Only edges whose label appears in `labels` are processed.

---

## Configuration File

The configuration file uses a key=value format, with one pair per line and **no spaces around the `=` sign**.

### Example Configuration

```ini
adaptive=10
input_data_path=code/dataset/higgs-activity/higgs-activity_time_postprocess.txt
size=2400
slide=200
query_type=1
labels=1
max_size=2400
min_size=1200
```

---

## Configuration Parameters

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `input_data_path` | String | Yes | Relative or absolute path to the edge list file.  Resolved against the current working directory. |
| `adaptive` | Integer | Yes | Windowing mode selector.  Determines how windows are managed (see [Windowing Modes](#windowing-modes) below). |
| `size` | Integer | Yes | Initial time window width (in timestamp units). Also used as the minimum length for ADWIN.  |
| `slide` | Integer | Yes | Slide step for window advancement. If equal to `size`, behaves as a tumbling window. |
| `max_size` | Integer | Yes | Upper bound for adaptive window size. Must be greater than or equal to `min_size`. |
| `min_size` | Integer | Yes | Lower bound for adaptive window size. Must be less than or equal to `max_size`. |
| `query_type` | Integer | Yes | Query identifier that selects which Regular Path Query automaton to build (see [Supported Queries](#supported-queries)). |
| `labels` | String | Yes | Comma-separated integers representing the ordered symbols used in automaton transitions. |

---

## Windowing Modes

The `adaptive` parameter controls which windowing strategy is used:

| Value | Mode Name | Description |
|-------|-----------|-------------|
| `10` | `sl` | **Sliding Window** - Fixed-size sliding window with constant size and slide parameters. |
| `11` | `ad_function` | **Adaptive ALEF** - Adaptive windowing based on an amortized load estimation function. |
| `12` | `ad_degree` | **Adaptive Degree** - Adaptive windowing based on nodes' out-degree centrality average. |
| `13` | `ad_einit` | **Adaptive LEF** - Adaptive windowing using a load estimation function. |
| `14` | `ad_freeman` | **Adaptive Max Degree** - Adaptive windowing based on the max nodes' out-degree centrality. |
| `15` | `ad_complexity` | **Adaptive Complexity** - Adaptive windowing based on the algorithmic complexity of RPQs. |
| `2` | `adwin` | **ADWIN** - Adaptive Windowing algorithm for detecting change and adjusting window size dynamically. |
| `3` | `lshed` | **Load Shedding** - Probabilistic load shedding mode that drops edges based on system load. |

### Mode Details

#### Sliding Window (`adaptive=10`)
Standard sliding window approach where:
- Window size remains fixed at `size`
- Windows slide by `slide` units
- No adaptive behavior

#### Adaptive Modes (`adaptive=11, 12, 13, 14, 15`)
These modes dynamically adjust the window size between `min_size` and `max_size` based on different heuristics.

#### ADWIN (`adaptive=2`)
Implements the ADWIN (ADaptive WINdowing) algorithm:
- Automatically detects concept drift
- Adjusts window size based on detected changes
- Uses configurable parameters:  `maxBuckets`, `minLen`, `delta`

#### Load Shedding (`adaptive=3`)
Probabilistic edge dropping mode:
- Granularity: `min_size / 100.0`
- Maximum shedding step: `max_size / 100.0`
- Edges are randomly dropped based on computed shedding probability

---

## Supported Queries (`query_type`)

| `query_type` | Pattern | Description |
|--------------|---------|-------------|
| 1 | `a+` | One or more occurrences of label `a` |
| 2 | `ab*` | Label `a` followed by zero or more `b` |
| 3 | `ab*c*` | Label `a`, then zero or more `b`, then zero or more `c` |
| 4 | `(abc)+` | One or more repetitions of the sequence `abc` |
| 5 | `ab*c` | Label `a`, zero or more `b`, then exactly one `c` |
| 6 | `a*b*` | Zero or more `a` followed by zero or more `b` |
| 7 | `abc*` | Labels `a` and `b` followed by zero or more `c` |
| 10 | `(a\|b)c*` | Either `a` or `b`, followed by zero or more `c` |

> **Note**:  Ensure the number of provided labels matches the distinct symbols required by the chosen query pattern.

---

## Generated CSV Outputs

Output files are named with the dataset folder name as prefix, followed by type and configuration parameters.

### 1. Summary Results (`_summary_results_...csv`)

| Column | Description |
|--------|-------------|
| `total_edges` | Number of processed edges (filtered by allowed labels) |
| `matches` | Total number of recognized paths |
| `exec_time` | Total execution time in seconds |
| `windows_created` | Number of windows generated |
| `avg_window_cardinality` | Average number of edges per window |
| `avg_window_size` | Average effective time span (for adaptive modes) |

### 2. Window Results (`_window_results_...csv`)

| Column | Description |
|--------|-------------|
| `index` | Window index |
| `t_open` | Window open timestamp |
| `t_close` | Window close timestamp |
| `window_results` | Results emitted at window close |
| `incremental_matches` | Cumulative matches up to this window |
| `latency` | Window completion latency |
| `window_cardinality` | Number of elements in the window |
| `window_size` | Time span of the window |

### 3. Tuples Results (`_tuples_results_...csv`)

| Column | Description |
|--------|-------------|
| `estimated_cost` | Estimated processing cost |
| `normalized_estimated_cost` | Normalized cost value |
| `latency` | Processing latency |
| `normalized_latency` | Normalized latency value |
| `window_cardinality` | Number of elements in window |
| `window_size` | Size of the window |

### 4. Memory Results (`_memory_results_...csv`)

> **Note**: Only generated if `MEMORY_PROFILER` is set to `true` at compile time (Linux only).

| Column | Description |
|--------|-------------|
| `tot_virtual` | Total virtual memory |
| `used_virtual` | Used virtual memory |
| `tot_ram` | Total RAM |
| `used_ram` | Used RAM |
| `data_mem` | Memory used by main data structures |

---

## Important Notes

1. **No comments** are supported in the configuration file.
2. **Do not insert spaces** around the `=` sign.
3. **Timestamps should be non-decreasing** for predictable behavior.
4. **`max_size` must be ≥ `min_size`** - the program will exit with an error if this constraint is violated.
5. **Memory profiling** is only supported on Linux and is disabled by default (`MEMORY_PROFILER false` in `main.cpp`).
