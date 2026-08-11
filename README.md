# Robust Regular Path Queries over Streaming Graphs via Load-Aware Windowing

## Additional materials

### High-load localised Speedup and Recall vs Load Shedding

#### Higgs

| Recall | Q1 a* | Q2 ab* | Q5 ab*c | Q7 abc* | Q4 (abc)+ | Q10 (a\|b)c* | Q6 a*b* | Q3 ab*c* |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Adaptive Window 75% | 0.76 | 0.69 | 0.69 | 0.68 | 0.70 | 0.69 | 0.75 | 0.69 |
| Adaptive Window 70% | 0.69 | 0.62 | 0.62 | 0.60 | 0.63 | 0.62 | 0.68 | 0.62 |
| Prob Shedding 5% | 0.86 (-0.08 / +0.03) | 0.86 (-0.01 / +0.01) | 0.85 (-0.15 / +0.06) | 0.88 (-0.07 / +0.03) | 0.86 (-0.01 / +0.01) | 0.86 (-0.15 / +0.06) | 0.88 (-0.03 / +0.03) | 0.88 (-0.13 / +0.05) |
| Prob Shedding 8% | 0.80 (-0.08 / +0.03) | 0.80 (-0.01 / +0.01) | 0.78 (-0.15 / +0.05) | 0.80 (-0.10 / +0.04) | 0.80 (-0.01 / +0.01) | 0.80 (-0.15 / +0.06) | 0.82 (-0.04 / +0.04) | 0.82 (-0.13 / +0.05) |
| Random Shedding 5% | 0.86 (-0.08 / +0.03) | 0.86 (-0.01 / +0.01) | 0.85 (-0.15 / +0.06) | 0.87 (-0.07 / +0.03) | 0.86 (-0.01 / +0.01) | 0.86 (-0.15 / +0.06) | 0.87 (-0.03 / +0.02) | 0.88 (-0.13 / +0.05) |
| Random Shedding 8% | 0.80 (-0.08 / +0.03) | 0.80 (-0.01 / +0.01) | 0.78 (-0.15 / +0.05) | 0.80 (-0.10 / +0.04) | 0.80 (-0.01 / +0.01) | 0.80 (-0.15 / +0.05) | 0.82 (-0.03 / +0.04) | 0.82 (-0.13 / +0.05) |

| Speedup | Q1 a* | Q2 ab* | Q5 ab*c | Q7 abc* | Q4 (abc)+ | Q10 (a\|b)c* | Q6 a*b* | Q3 ab*c* |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Adaptive Window 75% | 1.12 | 1.71 | 1.44 | 1.40 | 1.38 | 1.42 | 1.30 | 1.68 |
| Adaptive Window 70% | 1.18 | 1.90 | 1.62 | 1.54 | 1.60 | 1.59 | 1.46 | 1.84 |
| Prob Shedding 5% | 1.16 (-0.01 / +0.03) | 1.36 (-0.13 / +0.09) | 1.17 (-0.19 / +0.25) | 1.10 (-0.17 / +0.14) | 1.11 (-0.13 / +0.11) | 1.08 (-0.21 / +0.36) | 1.13 (-0.07 / +0.03) | 1.32 (-0.21 / +0.29) |
| Prob Shedding 8% | 1.27 (-0.03 / +0.02) | 1.49 (-0.13 / +0.11) | 1.29 (-0.23 / +0.34) | 1.24 (-0.19 / +0.21) | 1.23 (-0.12 / +0.12) | 1.24 (-0.24 / +0.31) | 1.25 (-0.03 / +0.03) | 1.43 (-0.24 / +0.35) |
| Random Shedding 5% | 1.17 (-0.01 / +0.02) | 1.38 (-0.12 / +0.06) | 1.19 (-0.21 / +0.24) | 1.13 (-0.19 / +0.15) | 1.19 (-0.13 / +0.06) | 1.17 (-0.23 / +0.29) | 1.17 (-0.07 / +0.07) | 1.35 (-0.23 / +0.24) |
| Random Shedding 8% | 1.29 (-0.03 / +0.04) | 1.51 (-0.16 / +0.12) | 1.30 (-0.24 / +0.30) | 1.23 (-0.20 / +0.19) | 1.30 (-0.11 / +0.05) | 1.26 (-0.24 / +0.32) | 1.30 (-0.06 / +0.12) | 1.47 (-0.26 / +0.35) |

<img width="3094" height="1125" alt="comparison_higgs-activity_high_load_periods" src="https://github.com/user-attachments/assets/d46eb211-ddad-4385-a406-c9e11bec9a88" />


#### LDBC

| Recall | Q1 a* | Q2 ab* | Q5 ab*c | Q7 abc* | Q4 (abc)+ | Q10 (a\|b)c* |
|---|---:|---:|---:|---:|---:|---:|
| Adaptive Window 60% | 1.00 | 0.99 | 0.99 | 0.98 | 1.01 | 0.99 |
| Adaptive Window 55% | 1.00 | 0.99 | 0.99 | 0.97 | 1.03 | 0.98 |
| Prob Shedding 5% | 0.94 (-0.00 / +0.00) | 0.94 (-0.00 / +0.00) | 0.91 (-0.00 / +0.00) | 0.91 (-0.00 / +0.00) | 0.91 (-0.01 / +0.01) | 0.87 (-0.02 / +0.01) |
| Prob Shedding 8% | 0.91 (-0.00 / +0.00) | 0.91 (-0.00 / +0.00) | 0.87 (-0.00 / +0.00) | 0.86 (-0.00 / +0.00) | 0.87 (-0.01 / +0.01) | 0.82 (-0.02 / +0.02) |
| Random Shedding 5% | 0.92 (-0.00 / +0.00) | 0.92 (-0.00 / +0.00) | 0.89 (-0.00 / +0.00) | 0.88 (-0.00 / +0.00) | 0.91 (-0.00 / +0.00) | 0.84 (-0.01 / +0.01) |
| Random Shedding 8% | 0.89 (-0.00 / +0.00) | 0.89 (-0.00 / +0.00) | 0.84 (-0.00 / +0.00) | 0.82 (-0.00 / +0.00) | 0.86 (-0.01 / +0.00) | 0.76 (-0.02 / +0.01) |

| Speedup | Q1 a* | Q2 ab* | Q5 ab*c | Q7 abc* | Q4 (abc)+ | Q10 (a\|b)c* |
|---|---:|---:|---:|---:|---:|---:|
| Adaptive Window 60% | 1.19 | 1.26 | 1.22 | 1.23 | 1.16 | 1.02 |
| Adaptive Window 55% | 1.17 | 1.22 | 1.24 | 1.25 | 1.17 | 1.00 |
| Prob Shedding 5% | 1.11 (-0.04 / +0.02) | 1.10 (-0.05 / +0.05) | 1.08 (-0.07 / +0.09) | 1.14 (-0.08 / +0.08) | 1.27 (-0.10 / +0.10) | 1.18 (-0.05 / +0.11) |
| Prob Shedding 8% | 1.13 (-0.06 / +0.05) | 1.10 (-0.03 / +0.07) | 1.11 (-0.09 / +0.07) | 1.19 (-0.09 / +0.09) | 1.38 (-0.13 / +0.15) | 1.33 (-0.11 / +0.05) |
| Random Shedding 5% | 1.12 (-0.04 / +0.06) | 1.13 (-0.07 / +0.08) | 1.11 (-0.07 / +0.05) | 1.18 (-0.08 / +0.08) | 1.30 (-0.11 / +0.10) | 1.27 (-0.09 / +0.09) |
| Random Shedding 8% | 1.16 (-0.04 / +0.06) | 1.18 (-0.10 / +0.09) | 1.16 (-0.08 / +0.10) | 1.25 (-0.09 / +0.09) | 1.41 (-0.12 / +0.17) | 1.40 (-0.08 / +0.08) |

<img width="3166" height="1125" alt="comparison_ldbc_high_load_periods" src="https://github.com/user-attachments/assets/60b663cf-1a2a-43cd-960e-fe523ab00f26" />

#### StackOverflow

| Recall | Q1 a* | Q2 ab* | Q5 ab*c | Q7 abc* | Q4 (abc)+ | Q10 (a\|b)c* | Q6 a*b* | Q3 ab*c* |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Adaptive Window 85% | 0.64 | 0.85 | 0.66 | 0.65 | 0.99 | 0.67 | 0.80 | 0.67 |
| Adaptive Window 80% | 0.54 | 0.55 | 0.55 | 0.53 | 0.78 | 0.56 | 0.75 | 0.56 |
| Prob Shedding 10% | 0.88 (-0.01 / +0.01) | 0.88 (-0.00 / +0.01) | 0.89 (-0.00 / +0.00) | 0.88 (-0.00 / +0.00) | 0.93 (-0.00 / +0.00) | 0.89 (-0.00 / +0.00) | 0.88 (-0.00 / +0.01) | 0.90 (-0.00 / +0.00) |
| Prob Shedding 15% | 0.82 (-0.01 / +0.01) | 0.81 (-0.01 / +0.01) | 0.81 (-0.02 / +0.02) | 0.81 (-0.02 / +0.02) | 0.89 (-0.00 / +0.00) | 0.82 (-0.01 / +0.02) | 0.83 (-0.01 / +0.01) | 0.83 (-0.01 / +0.02) |
| Random Shedding 10% | 0.79 (-0.00 / +0.00) | 0.80 (-0.01 / +0.01) | 0.79 (-0.01 / +0.01) | 0.78 (-0.01 / +0.01) | 0.84 (-0.00 / +0.00) | 0.80 (-0.01 / +0.01) | 0.79 (-0.01 / +0.00) | 0.81 (-0.01 / +0.00) |
| Random Shedding 15% | 0.69 (-0.01 / +0.01) | 0.70 (-0.01 / +0.01) | 0.68 (-0.01 / +0.01) | 0.67 (-0.01 / +0.01) | 0.76 (-0.00 / +0.00) | 0.70 (-0.00 / +0.01) | 0.70 (-0.01 / +0.01) | 0.71 (-0.00 / +0.01) |

| Speedup | Q1 a* | Q2 ab* | Q5 ab*c | Q7 abc* | Q4 (abc)+ | Q10 (a\|b)c* | Q6 a*b* | Q3 ab*c* |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Adaptive Window 85% | 1.68 | 1.36 | 1.71 | 1.78 | 1.08 | 1.83 | 1.35 | 1.71 |
| Adaptive Window 80% | 2.06 | 2.33 | 2.24 | 2.19 | 1.43 | 2.25 | 1.53 | 2.19 |
| Prob Shedding 10% | 1.01 (-0.03 / +0.02) | 1.03 (-0.02 / +0.02) | 1.05 (-0.04 / +0.07) | 1.06 (-0.01 / +0.01) | 0.96 (-0.02 / +0.02) | 1.03 (-0.01 / +0.00) | 1.06 (-0.04 / +0.03) | 0.99 (-0.01 / +0.00) |
| Prob Shedding 15% | 1.15 (-0.07 / +0.20) | 1.23 (-0.09 / +0.26) | 1.22 (-0.10 / +0.29) | 1.25 (-0.07 / +0.19) | 1.05 (-0.04 / +0.06) | 1.24 (-0.11 / +0.29) | 1.19 (-0.04 / +0.13) | 1.16 (-0.08 / +0.23) |
| Random Shedding 10%  | 1.16 (-0.01 / +0.01) | 1.19 (-0.03 / +0.02) | 1.18 (-0.03 / +0.05) | 1.19 (-0.01 / +0.01) | 1.11 (-0.03 / +0.03) | 1.17 (-0.04 / +0.02) | 1.14 (-0.02 / +0.02) | 1.16 (-0.01 / +0.02) |
| Random Shedding 15% | 1.41 (-0.07 / +0.25) | 1.52 (-0.13 / +0.31) | 1.49 (-0.08 / +0.25) | 1.53 (-0.09 / +0.23) | 1.41 (-0.12 / +0.22) | 1.50 (-0.08 / +0.28) | 1.43 (-0.08 / +0.20) | 1.46 (-0.07 / +0.26) |

<img width="3166" height="1125" alt="comparison_so_high_load_periods" src="https://github.com/user-attachments/assets/f9e04542-1522-4f2c-9ca1-cfa6269343bb" />


### Extensive Experiments
We report the experiments of latency, throughput and completeness for all the mentioned Regular Path Queries and for the additional dataset [StackOverflow](https://snap.stanford.edu/data/sx-stackoverflow.html). In addition, we report a comparison study with the adaptive window foundational work [ADWIN](https://scholar.google.com/citations?view_op=view_citation&hl=it&user=UYvAL8EAAAAJ&citation_for_view=UYvAL8EAAAAJ:XiSMed-E-HIC).

#### StackOverflow
We execute the experiments on the first 800 days of the dataset and we introuce syntetic peaks. We maintain the same edges to preserve the real-world dataset relationships, but we increase the input rate by 4x between 200 and 300 days, and 3x between 500 and 600 days. We decrease the input rate in the remaining temporal ranges to maitain a total time span of 800 days.

###### Speedup and Recall
<img width="3616" height="1058" alt="comparison_so_combined" src="https://github.com/user-attachments/files/29243183/window_recall_speedup.pdf" />

###### CCDF
<img width="4856" height="1063" alt="comparison_so_speedup_recall" src="https://github.com/user-attachments/files/29250434/ccdf_latency_log.pdf" />

###### Recall vs Load Shedding

| Recall       | Q1 a*                | Q2 ab*               | Q5 ab*c              | Q7 abc*              | Q4 (abc)+            | Q10 (a\|b)c*         | Q6 a* b*                | Q3 ab* c*              |
|---------------------|----------------------|----------------------|----------------------|----------------------|----------------------|----------------------|----------------------|----------------------|
| Adaptive Window 85% | 0.77                 | **0.92**                 | 0.77                 | 0.76                 | **0.92**                 | 0.79                 | **0.89**                 | 0.79                 |
| Adaptive Window 80% | 0.70                 | 0.70                 | 0.69                 | 0.67                 | 0.89                 | 0.71                 | 0.85                 | 0.71                 |
| Random Shedding 10% | 0.79 (-0.00 / +0.00) | 0.80 (-0.01 / +0.01) | 0.79 (-0.01 / +0.01) | 0.78 (-0.01 / +0.01) | 0.80 (-0.00 / +0.00) | 0.80 (-0.01 / +0.01) | 0.80 (-0.01 / +0.01) | 0.81 (-0.01 / +0.01) |
| Random Shedding 15% | 0.70 (-0.01 / +0.01) | 0.70 (-0.01 / +0.01) | 0.68 (-0.01 / +0.01) | 0.67 (-0.01 / +0.01) | 0.71 (-0.00 / +0.00) | 0.70 (-0.00 / +0.01) | 0.70 (-0.01 / +0.01) | 0.71 (-0.00 / +0.01) |
| Prob Shedding 10%   | 0.88 (-0.01 / +0.01) | 0.88 (-0.00 / +0.00) | 0.88 (-0.01 / +0.01) | 0.88 (-0.01 / +0.01) | 0.89 (-0.00 / +0.00) | 0.89 (-0.01 / +0.00) | 0.88 (-0.01 / +0.01) | 0.90 (-0.01 / +0.00) |
| Prob Shedding 15%   | 0.83 (-0.01 / +0.00) | 0.82 (-0.01 / +0.01) | 0.82 (-0.01 / +0.01) | 0.81 (-0.01 / +0.01) | 0.85 (-0.00 / +0.00) | 0.83 (-0.01 / +0.01) | 0.84 (-0.00 / +0.01) | 0.84 (-0.01 / +0.01) |

| Speedup       | Q1 a*                | Q2 ab*               | Q5 ab*c              | Q7 abc*              | Q4 (abc)+            | Q10 (a\|b)c*         | Q6 ab                | Q3 abc               |
|---------------------|----------------------|----------------------|----------------------|----------------------|----------------------|----------------------|----------------------|----------------------|
| Adaptive Window 85% | 1.42                 | **1.22**                 | 1.46                 | 1.51                 | **1.19**                 | 1.53                 | **1.21**                 | 1.46                 |
| Adaptive Window 80% | 1.62                 | 1.80                 | 1.76                 | 1.73                 | 1.27                 | 1.75                 | 1.32                 | 1.73                 |
| Random Shedding 10% | 1.20 (-0.06 / +0.18) | 1.24 (-0.10 / +0.28) | 1.22 (-0.07 / +0.19) | 1.23 (-0.06 / +0.18) | 1.16 (-0.07 / +0.15) | 1.23 (-0.10 / +0.26) | 1.17 (-0.06 / +0.16) | 1.20 (-0.05 / +0.17) |
| Random Shedding 15% | 1.40 (-0.07 / +0.24) | 1.50 (-0.13 / +0.30) | 1.47 (-0.09 / +0.26) | 1.51 (-0.09 / +0.23) | 1.43 (-0.12 / +0.23) | 1.49 (-0.08 / +0.28) | 1.42 (-0.07 / +0.20) | 1.45 (-0.07 / +0.26) |
| Prob Shedding 10%   | 1.04 (-0.06 / +0.15) | 1.07 (-0.07 / +0.21) | 1.08 (-0.07 / +0.15) | 1.09 (-0.05 / +0.13) | 1.01 (-0.06 / +0.16) | 1.08 (-0.07 / +0.24) | 1.08 (-0.05 / +0.08) | 1.03 (-0.06 / +0.20) |
| Prob Shedding 15%   | 1.14 (-0.06 / +0.19) | 1.22 (-0.09 / +0.26) | 1.20 (-0.10 / +0.29) | 1.24 (-0.06 / +0.18) | 1.06 (-0.04 / +0.06) | 1.23 (-0.11 / +0.28) | 1.18 (-0.05 / +0.13) | 1.15 (-0.08 / +0.23) |

<img width="3644" height="1063" alt="comparison_so_speedup_recall" src="https://github.com/user-attachments/assets/56feafb8-8126-42df-90e2-092fb255b471" />

###### Ablation study

#### Comparison vs ADWIN

<img width="3616" height="1058" alt="comparison_so_combined" src="https://github.com/user-attachments/files/28843637/all_vs_adwin.pdf" />

ADWIN maintains stable load-estimation values across all delta configurations for both datasets, demonstrating its design goal of preserving the stability of the input distribution. However, because the estimation is overly influenced by the window size, it no longer guides window adaptation to the workload, resulting in small windows. Because ADWIN prioritizes maintaining statistical stability, the resulting windows do not provide sufficient time for edges to form meaningful graph structures and connections. Indeed, the inability to specify a window size makes ADWIN unsuitable for scaling to realistic streaming graph-processing workloads.


## Code Layout

The current executable lives under `code/`:

- `code/main.cpp`: entry point, configuration parsing, stream loop, CSV output.
- `code/source/S-PATH.h`: S-PATH implementation, including dedicated mid-window edge deletion for load shedding.
- `code/source/LM-SRPQ.h`: LM-SRPQ implementation and dynamic landmark maintenance.
- `code/source/query_processor.h`: path algorithm factory. Current algorithm IDs are `1` for S-PATH and `2` for LM-SRPQ.
- `code/source/modes/`: sliding-window, adaptive, ADWIN, and load-shedding mode handlers.
- `code/source/streaming_graph.h`: streaming graph, timed edge list, forward adjacency list, and reverse adjacency list.
- `code/benchmark/config/`: experiment configuration files.
- `code/benchmark/results/`: generated experiment outputs.

## Build

Prerequisites: a C++17 compiler and CMake 3.10 or newer.

```bash
cmake -S . -B build
cmake --build build
```

The default executable is:

```bash
./build/main_exe
```

## Run

The program accepts one argument: the path to a configuration file.

```bash
./build/main_exe code/config.txt
```

## Input Graph File Format

The file referenced by `input_data_path` must contain one edge per line:

```text
s d l t
```

Where:

- `s`: source vertex ID.
- `d`: destination vertex ID.
- `l`: integer edge label.
- `t`: timestamp. Timestamps are expected to be non-decreasing.

Only edges whose label appears in the selected RPQ automaton are processed.

## Configuration File

Configuration files use `key=value`, one pair per line, with no spaces around `=`.

### Example Configuration

```ini
mode=10
input_data_path=code/dataset/ldbc/ldbc_updatestream_sf10_peaks.txt
size=1036800
slide=21600
query_type=10
labels=3,4,1
path_algorithm=2
max_size=1036800
min_size=518400
rate_volatility=0.05
output_folder=results
```

## Configuration Parameters

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `mode` | Integer | Yes | Windowing or load-shedding mode selector. See [Modes](#modes). |
| `input_data_path` | String | Yes | Relative or absolute path to the edge-list file. Relative paths are resolved from the current working directory. |
| `size` | Integer | Yes | Initial or fixed time-window width, in timestamp units. |
| `slide` | Integer | Yes | Slide step for window advancement. If equal to `size`, the run behaves as a tumbling window. |
| `query_type` | Integer | Yes | RPQ automaton selector. See [Supported Queries](#supported-queries-query_type). |
| `labels` | String | Yes | Comma-separated label IDs, in the order used by the query pattern. |
| `path_algorithm` | Integer | Yes | Path processor selector. `1` = S-PATH, `2` = LM-SRPQ. |
| `max_size` | Integer | Modes `10`-`15` | Maximum window size. For fixed sliding windows, set this to `size`. |
| `min_size` | Integer | Modes `10`-`15` | Minimum window size. For fixed sliding windows, set this to the desired lower bound, often `size` or a tuned adaptive minimum. |
| `rate_volatility` | Float | Optional for modes `11`-`15` | Adaptation sensitivity. Defaults to `0.01`. Must be greater than `0` when provided. |
| `min_variation` | Float | Optional for modes `11`-`15` | Minimum normalized change needed before resizing. Must be in `(0, 1]`. |
| `load_average_horizon` | Integer | Optional for modes `11`-`15` | Number of recent normalized-cost values used for smoothing. Defaults to the window overlap. |
| `normalization_horizon` | Integer | Optional for modes `11`-`15` | Number of recent costs used for min/max normalization. `0` means all history. |
| `granularity` | Float | Modes `3` and `4` | Load-shedding step as a fraction, for example `0.05` for 5%. |
| `max_shed` | Float | Modes `3` and `4` | Maximum shedding probability/fraction as a fraction, for example `0.08` for 8%. |
| `l_max` | Float | Mode `5` | Latency budget for DARLING-style ranked shedding. |
| `output_folder` | String | Optional | Folder where CSV files are written. Defaults to `results`. |

## Modes

The `mode` parameter controls how windows are maintained and whether edges may be shed.

| Value | Output Tag | Description |
|-------|------------|-------------|
| `10` | `sl` | Fixed sliding window baseline. |
| `11` | `ad_function` | Adaptive ALEF windowing using the normalized amortized load estimate. |
| `12` | `ad_degree` | Adaptive windowing using average degree. |
| `13` | `ad_einit` | Adaptive LEF windowing using initial-transition edge load. |
| `14` | `maxdeg` | Adaptive windowing using maximum out-degree. |
| `15` | `ad_complexity` | Adaptive windowing using the `EINIT * edge_count` complexity estimate. |
| `2` | `adwin` | ADWIN-based adaptive windowing. |
| `3` | `lshed_prob` | Probabilistic load shedding with adaptive shedding probability. |
| `4` | `lshed_random` | Random load shedding with fixed maximum shedding probability. |
| `5` | `lshed_darling` | Ranked load shedding driven by the latency budget `l_max`. |

For modes `11`-`15`, the current cost components are written to the functions CSV on each cost computation: ALEF, average degree, LEF, max degree, and `nm`.

## Path Algorithms

| `path_algorithm` | Name | Notes |
|------------------|------|-------|
| `1` | S-PATH | Maintains prefix spanning trees over the product graph. Supports dedicated `shed_edges` handling for mid-window edge deletion. |
| `2` | LM-SRPQ | Maintains landmark-aware RPQ state and runs dynamic landmark selection during expiration. |

S-PATH's load-shedding path no longer treats shed edges as ordinary window expiration. It identifies tree nodes whose parent link used the deleted edge, searches incoming live edges through the reverse adjacency list, reconnects to the best surviving parent when possible, and deletes the affected subtree only when no alternative path exists.

## Supported Queries (`query_type`)

| `query_type` | Pattern | Required Labels | Description |
|--------------|---------|-----------------|-------------|
| `1` | `a+` | 1 | One or more occurrences of `a`. |
| `2` | `ab*` | 2 | `a` followed by zero or more `b`. |
| `3` | `ab*c*` | 3 | `a`, then zero or more `b`, then zero or more `c`. |
| `4` | `(abc)+` | 3 | One or more repetitions of `abc`. |
| `5` | `ab*c` | 3 | `a`, zero or more `b`, then exactly one `c`. |
| `6` | `a*b*` | 2 | Zero or more `a` followed by zero or more `b`. |
| `7` | `abc*` | 3 | `a`, then `b`, then zero or more `c`. |
| `8` | `(a\|b)+c` | 3 | One or more `a`/`b` transitions followed by `c`. |
| `10` | `(a\|b)c*` | 3 | Either `a` or `b`, followed by zero or more `c`. |

The `labels` list maps pattern symbols by position. For example, `query_type=10` with `labels=3,4,1` means `(3|4)1*`.

## Generated CSV Outputs

Output files are written under `output_folder`. For the main modes, the filename pattern is:

```text
{dataset}_{query}_{size}_{slide}_{mode}_{path_algorithm}_{param1}_{param2}_{result_type}.csv
```

Parameter fields are:

- Fixed sliding window (`mode=10`): `0_0`.
- Adaptive modes (`mode=11`-`15`): `{min_size}_{max_size}`.
- Load shedding modes `3` and `4`: `{granularity%}_{max_shed%}`, for example `5_8`.

`mode=5` writes the same CSV types, but the current filename builder in `code/main.cpp` does not assign its parameterized base name yet.

### Summary Results (`summary_results`)

```text
total_edges,matches,exec_time,windows_created,avg_window_cardinality,avg_window_size
```

| Column | Description |
|--------|-------------|
| `total_edges` | Number of processed edges after label filtering. |
| `matches` | Total number of matched paths emitted into the sink. |
| `exec_time` | Total execution time in seconds. |
| `windows_created` | Number of windows created during the run. |
| `avg_window_cardinality` | Average number of edge memberships across windows. |
| `avg_window_size` | Average effective window size tracked by the mode handler. |

### Window Results (`window_results`)

```text
window_id,t_open,t_close,normalized_estimated_cost,window_results,incremental_matches,latency,window_cardinality,window_size
```

| Column | Description |
|--------|-------------|
| `window_id` | Window index. |
| `t_open` | Window open timestamp. |
| `t_close` | Window close timestamp. |
| `normalized_estimated_cost` | Normalized cost assigned to this window. |
| `window_results` | Results emitted between this window's open and close snapshots. |
| `incremental_matches` | Cumulative matched paths at window close. |
| `latency` | Window completion latency. |
| `window_cardinality` | Number of elements assigned to the window. |
| `window_size` | `t_close - t_open`. |

### Slide Results (`slides_results`)

```text
t_open,t_close,latency_sec,elements,new_results,cost_norm
```

| Column | Description |
|--------|-------------|
| `t_open` | Slide open timestamp. |
| `t_close` | Slide close timestamp. |
| `latency_sec` | Wall-clock latency for the slide interval. |
| `elements` | Edges seen in the slide interval. |
| `new_results` | Matches emitted during the slide interval. |
| `cost_norm` | Normalized cost at the slide boundary. |

### Functions Results (`functions_results`)

```text
alef,avg_deg,lef,max_deg,nm
```

These are the cost components used by the adaptive modes and load-shedding control logic.

## Important Notes

1. Configuration files do not support comments.
2. Do not insert spaces around `=`.
3. Timestamps should be non-decreasing for predictable window expiration.
4. For modes `10`-`15`, `max_size` must be greater than or equal to `min_size`.
5. `tuples_results` output is currently disabled in `code/main.cpp`.
6. `MEMORY_PROFILER` is currently set to `false`; the active auxiliary output is `functions_results`, not a memory-profiler CSV.
