import os

QUERY_LABELS = {
    "ldbc": {
        1: [3],
        2: [2, 3],
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
        2: [2, 1],
        3: [3, 1, 2],
        5: [2, 1, 3],
        6: [1, 2],
        7: [1, 3, 2],
        10: [2, 3, 1],
    },
    "so_4": {
        4: [1, 3, 2],
    },
    "ldbc_4": {
        4: [1, 2, 4],
    },

    "so_new": {
        2: [2, 1],
        5: [2, 1, 3],
        7: [1, 3, 2]
    }
}


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def format_load_shedding_filename_value(value):
    # Keep whole percentages unchanged and remove the decimal separator otherwise.
    value_text = format(float(value), "f").rstrip("0").rstrip(".")
    return value_text.replace(".", "")


def build_base_content(
        algorithm,
        dataset,
        size,
        slide,
        query_type,
        labels,
        path_algorithm,
        load_average_horizon,
        min_variation,
):
    # Include adaptation defaults in every generated configuration.
    return (
        f"mode={algorithm}\n"
        f"input_data_path={dataset}\n"
        f"size={size}\n"
        f"slide={slide}\n"
        f"query_type={query_type}\n"
        f"labels={','.join(map(str, labels))}\n"
        f"path_algorithm={path_algorithm}\n"
        f"load_average_horizon={load_average_horizon}\n"
        f"min_variation={min_variation}\n"
    )


def generate_config_files(
        datasets,
        query_label_pairs,
        output,
        algorithms,
        size,
        slide,
        path_algorithm=2, #lm-srpq
        load_average_horizon=0,         # 0 uses the runtime overlap-based default
        min_variation=0.02,             # default minimum relative cost variation
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
                    load_average_horizon=load_average_horizon,
                    min_variation=min_variation,
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

                    for granularity, max_shed in load_shedding_params:
                        # Parameters are specified as percentages but consumed as probabilities.
                        granularity_probability = float(granularity) / 100
                        max_shed_probability = float(max_shed) / 100
                        config_content = (
                                base
                                + f"granularity={granularity_probability:g}\n"
                                + f"max_shed={max_shed_probability:g}\n"
                        )

                        g_pct = format_load_shedding_filename_value(granularity)
                        ms_pct = format_load_shedding_filename_value(max_shed)

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
    ldbc_query_4_label_pairs = [(q, QUERY_LABELS["ldbc_4"][q]) for q in QUERY_LABELS["ldbc_4"]]
    higgs_query_label_pairs = [(q, QUERY_LABELS["higgs"][q]) for q in QUERY_LABELS["higgs"]]
    so_query_label_pairs = [(q, QUERY_LABELS["so"][q]) for q in QUERY_LABELS["so"]]
    so_new_query_label_pairs = [(q, QUERY_LABELS["so_new"][q]) for q in QUERY_LABELS["so_new"]]
    so_query_4_label_pairs = [(q, QUERY_LABELS["so_4"][q]) for q in QUERY_LABELS["so_4"]]

    ldbc = {
        "datasets": ["code/dataset/ldbc/ldbc_updatestream_sf10_peaks.txt"],
        "query_label_pairs": ldbc_query_label_pairs,
        "size": 1036800,
        "slide": 21600,
        "load_average_horizon": 0,
        "min_variation": 0.02,
        "min_size_percentages": [50, 65, 70],
        "load_shedding_params": [(6, 6), (7, 7), (9, 9)],
    }

    ldbc_4 = {
        "datasets": ["code/dataset/ldbc/ldbc_updatestream_sf10_peaks.txt"],
        "query_label_pairs": ldbc_query_4_label_pairs,
        "size": 518400,
        "slide": 21600,
        "load_average_horizon": 0,
        "min_variation": 0.02,
        "min_size_percentages": [],
        "load_shedding_params": [],
    }

    higgs = {
        "datasets": ["code/dataset/higgs-activity/higgs-activity_time_postprocess.txt"],
        "query_label_pairs": higgs_query_label_pairs,
        "size": 172800,
        "slide": 2160,
        "load_average_horizon": 0,
        "min_variation": 0.02,
        "min_size_percentages": [],
        "load_shedding_params": [],
    }

    so = {
        "datasets": ["code/dataset/so/sx_stackoverflow_merged_peaks.txt"],
        "query_label_pairs": so_query_label_pairs,
        "size": 432000,
        "slide": 21600,
        "load_average_horizon": 22,
        "min_variation": 0.02,
        "min_size_percentages": [90,75],
        "load_shedding_params": [(11.5,11.5), (13,13)],
    }

    so_new = {
        "datasets": ["code/dataset/so/sx_stackoverflow_merged_peaks.txt"],
        "query_label_pairs": so_new_query_label_pairs,
        "size": 432000,
        "slide": 21600,
        "load_average_horizon": 22,
        "min_variation": 0.02,
        "min_size_percentages": [90,85,80,75],
        "load_shedding_params": [(10,10), (11.5,11.5), (13,13), (15,15)],
    }

    so_4 = {
        "datasets": ["code/dataset/so/sx_stackoverflow_merged_peaks.txt"],
        "query_label_pairs": so_query_4_label_pairs,
        "size": 216000,
        "slide": 21600,
        "load_average_horizon": 22,
        "min_variation": 0.02,
        "min_size_percentages": [],
        "load_shedding_params": [],
    }

    current_conf = so

    algorithms = [11,3,4]
    output = "sigmod/so"

    generate_config_files(
        datasets=current_conf["datasets"],
        query_label_pairs=current_conf["query_label_pairs"],
        output=output,
        algorithms=algorithms,
        size=current_conf["size"],
        slide=current_conf["slide"],
        load_average_horizon=current_conf["load_average_horizon"],
        min_variation=current_conf["min_variation"],
        min_size_percentages=current_conf["min_size_percentages"],
        load_shedding_params=current_conf["load_shedding_params"],
    )


if __name__ == "__main__":
    main()
