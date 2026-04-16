import os

QUERY_LABELS = {
    "ldbc": {
        1: [3],
        2: [2, 3],
        4: [1, 2, 4],
        5: [2, 3, 4],
        7: [4, 2, 3],
        10: [3, 4, 1],
    },
    "higgs": {
        1: [1],
        2: [1, 2],
        3: [1, 2, 3],
        4: [1, 2, 3],
        5: [1, 2, 3],
        6: [1, 3],
        7: [1, 3, 2],
        10: [1, 3, 2],
    },
    "so": {
        1: [1],
        2: [3, 1],
        3: [3, 1, 2],
        4: [1, 3, 2],
        5: [3, 1, 2],
        6: [1, 2],
        7: [3, 2, 1],
        10: [2, 3, 1],
    },
}


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def build_base_content(algorithm, dataset, size, slide, query_type, labels, path_algorithm):
    return (
        f"mode={algorithm}\n"
        f"input_data_path={dataset}\n"
        f"size={size}\n"
        f"slide={slide}\n"
        f"query_type={query_type}\n"
        f"labels={','.join(map(str, labels))}\n"
        f"path_algorithm={path_algorithm}\n"
    )


def generate_config_files(
        datasets,
        query_label_pairs,
        output,
        algorithms,
        size,
        slide,
        path_algorithm=2, #lm-srpq
        min_size_percentages=None,      # usato da algoritmo 11, es. [75, 80, 85]
        load_shedding_params=None,      # usato da algoritmo 3/4, es. [(15, 3), (10, 2)]
        l_max=-1.0,                     # usato da algoritmo 5
):
    if min_size_percentages is None:
        min_size_percentages = []
    if load_shedding_params is None:
        load_shedding_params = []

    out_dir = os.path.join("config", output)
    ensure_dir(out_dir)

    for dataset in datasets:
        for query_type, labels in query_label_pairs:
            for algorithm in algorithms:
                base = build_base_content(
                    algorithm=algorithm,
                    dataset=dataset,
                    size=size,
                    slide=slide,
                    query_type=query_type,
                    labels=labels,
                    path_algorithm=path_algorithm,
                )

                if algorithm == 10:
                    config_content = base + "max_size=0\nmin_size=0\n"
                    config_filename = (
                        f"config_a{algorithm}_S{size}_s{slide}_q{query_type}"
                        f"_p{path_algorithm}_M0_m0.txt"
                    )
                    config_filepath = os.path.join(out_dir, config_filename)
                    with open(config_filepath, "w") as config_file:
                        config_file.write(config_content)
                    print(f"Generated {config_filepath}")

                elif algorithm == 11:
                    if not min_size_percentages:
                        print("No percentages provided for algorithm 11; skipping.")
                        continue

                    for pct in min_size_percentages:
                        min_size = int(round(size * (pct / 100.0)))
                        max_size = size
                        config_content = (
                                base
                                + f"max_size={max_size}\n"
                                + f"min_size={min_size}\n"
                        )
                        config_filename = (
                            f"config_a{algorithm}_S{size}_s{slide}_q{query_type}"
                            f"_p{path_algorithm}_M{max_size}_m{min_size}.txt"
                        )
                        config_filepath = os.path.join(out_dir, config_filename)
                        with open(config_filepath, "w") as config_file:
                            config_file.write(config_content)
                        print(f"Generated {config_filepath}")

                elif algorithm in (3, 4):
                    if not load_shedding_params:
                        print(f"No load shedding params for algorithm {algorithm}; skipping.")
                        continue

                    def prob_to_pct_int(value):
                        v = float(value)
                        v = max(0.0, min(1.0, v))  # clamp
                        return int(round(v * 100))

                    for granularity, max_shed in load_shedding_params:
                        config_content = (
                                base
                                + f"granularity={granularity}\n"
                                + f"max_shed={max_shed}\n"
                        )

                        g_pct = prob_to_pct_int(granularity)
                        ms_pct = prob_to_pct_int(max_shed)

                        config_filename = (
                            f"config_a{algorithm}_S{size}_s{slide}_q{query_type}"
                            f"_p{path_algorithm}_g{g_pct}_ms{ms_pct}.txt"
                        )

                        config_filepath = os.path.join(out_dir, config_filename)
                        with open(config_filepath, "w") as config_file:
                            config_file.write(config_content)
                        print(f"Generated {config_filepath}")

                elif algorithm == 5:
                    config_content = base + f"l_max={l_max}\n"
                    config_filename = (
                        f"config_a{algorithm}_S{size}_s{slide}_q{query_type}"
                        f"_p{path_algorithm}_l{l_max}.txt"
                    )
                    config_filepath = os.path.join(out_dir, config_filename)
                    with open(config_filepath, "w") as config_file:
                        config_file.write(config_content)
                    print(f"Generated {config_filepath}")

                else:
                    print(f"Unsupported algorithm for configuration generation: {algorithm}")


def main():
    # Query-label pairs
    ldbc_query_label_pairs = [(q, QUERY_LABELS["ldbc"][q]) for q in QUERY_LABELS["ldbc"]]
    higgs_query_label_pairs = [(q, QUERY_LABELS["higgs"][q]) for q in QUERY_LABELS["higgs"]]
    so_query_label_pairs = [(q, QUERY_LABELS["so"][q]) for q in QUERY_LABELS["so"]]

    ldbc_conf = {
        "datasets": ["code/dataset/ldbc/ldbc_updatestream_sf10_peaks.txt"],
        "query_label_pairs": ldbc_query_label_pairs,
        "size": 864000,
        "slide": 43200,
        "min_size_percentages": [75, 80, 85],
        "load_shedding_params": [(0.03, 0.15), (0.02, 0.10), (0.01, 0.05)],
    }

    higgs_conf = {
        "datasets": ["code/dataset/higgs-activity/higgs-activity_time_postprocess.txt"],
        "query_label_pairs": higgs_query_label_pairs,
        "size": 86400,
        "slide": 2160,
        "min_size_percentages": [75, 80, 85],
        "load_shedding_params": [(0.03, 0.15), (0.02, 0.10), (0.01, 0.05)],
    }

    so_conf = {
        "datasets": ["code/dataset/so/sx_stackoverflow_merged_peaks.txt"],
        "query_label_pairs": so_query_label_pairs,
        "size": 432000,
        "slide": 21600,
        "min_size_percentages": [75, 80, 85],
        "load_shedding_params": [(0.03, 0.15), (0.02, 0.10), (0.01, 0.05)],
    }

    current_conf = {
        "datasets": ["code/dataset/ldbc/ldbc_updatestream_sf10_peaks.txt"],
        "query_label_pairs": ldbc_query_label_pairs,
        "size": 864000,
        "slide": 43200,
        "min_size_percentages": [75],
        "load_shedding_params": [(0.03, 0.15)],
    }

    algorithms = [10, 11, 3, 4]
    output = "icde/optimization/adaptive"

    generate_config_files(
        datasets=current_conf["datasets"],
        query_label_pairs=current_conf["query_label_pairs"],
        output=output,
        algorithms=algorithms,
        size=current_conf["size"],
        slide=current_conf["slide"],
        min_size_percentages=current_conf["min_size_percentages"],
        load_shedding_params=current_conf["load_shedding_params"],
    )


if __name__ == "__main__":
    main()
