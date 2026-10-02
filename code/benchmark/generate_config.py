import os

QUERY_LABELS = {
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

    "ldbc": {
        1: [3], # replyOf
        2: [2, 3], # likes, replyOf
        5: [2, 3, 4], # likes, replyOf, hasCreator
        7: [4, 2, 3], # hasCreator, likes, replyOf
        10: [6, 2, 3], # containerOf, likes, replyOf
    },
    "ldbc_4": {
        4: [1, 2, 4],
    },
    "ldbc_alternative": {
        1: [3],
        2: [2, 3],
        10: [6, 2, 3],
    },

    "so": {
        1: [1],
        2: [2, 1],
        5: [2, 1, 3],
        6: [1, 2],
        7: [1, 3, 2],
        10: [2, 3, 1],
    },
    "so_4": {
        3: [3, 1, 2],
        4: [1, 3, 2]
    },
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
        min_size_slide_multipliers=None,  # Used by algorithm 11, e.g. [15, 16, 17].
        prob_load_shedding_percentages=None,    # Used by algorithm 3, e.g. [6, 8].
        random_load_shedding_percentages=None,  # Used by algorithm 4, e.g. [10, 15].
        l_max=-1.0,                     # usato da algoritmo 5
):
    if min_size_slide_multipliers is None:
        min_size_slide_multipliers = []
    if prob_load_shedding_percentages is None:
        prob_load_shedding_percentages = []
    if random_load_shedding_percentages is None:
        random_load_shedding_percentages = []

    # Keep each load-shedding mode tied to its independently configured percentages.
    load_shedding_percentages_by_algorithm = {
        3: prob_load_shedding_percentages,
        4: random_load_shedding_percentages,
    }

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
                    if not min_size_slide_multipliers:
                        print("No minimum-size slide multipliers provided for algorithm 11; skipping.")
                        continue

                    for multiplier in min_size_slide_multipliers:
                        # Express the minimum window as a number of slide intervals.
                        min_size = int(round(slide * multiplier))
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
                    load_shedding_percentages = load_shedding_percentages_by_algorithm[algorithm]
                    if not load_shedding_percentages:
                        print(f"No load shedding percentages for algorithm {algorithm}; skipping.")
                        continue

                    for max_shed in load_shedding_percentages:
                        # Parameters are specified as percentages but consumed as probabilities.

                        max_shed_probability = float(max_shed) / 100
                        config_content = (
                                base
                                + f"granularity={max_shed_probability:g}\n"
                                + f"max_shed={max_shed_probability:g}\n"
                        )

                        g_pct = format_load_shedding_filename_value(max_shed)
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
    ldbc_query_alternative_label_pairs = [(q, QUERY_LABELS["ldbc_alternative"][q]) for q in QUERY_LABELS["ldbc_alternative"]]
    higgs_query_label_pairs = [(q, QUERY_LABELS["higgs"][q]) for q in QUERY_LABELS["higgs"]]
    so_query_label_pairs = [(q, QUERY_LABELS["so"][q]) for q in QUERY_LABELS["so"]]
    so_query_4_label_pairs = [(q, QUERY_LABELS["so_4"][q]) for q in QUERY_LABELS["so_4"]]

    higgs = {
        "datasets": ["code/dataset/higgs-activity/higgs-activity_time_postprocess.txt"],
        "query_label_pairs": higgs_query_label_pairs,
        "size": 172800,
        "slide": 2160,
        "load_average_horizon": 0,
        "min_variation": 0.02,
        "min_size_slide_multipliers": [],
        "random_load_shedding_percentages": [],
        "prob_load_shedding_percentages": [],
    }


    ldbc = {
        "datasets": ["code/dataset/ldbc/ldbc_updatestream_sf10_peaks.txt"],
        "query_label_pairs": ldbc_query_label_pairs,
        "size": 259200,
        "slide": 21600,
        "load_average_horizon": 0,
        "min_variation": 0.02,
        "min_size_slide_multipliers": [6, 7, 8, 9],
        "random_load_shedding_percentages": [8, 10, 12, 14],
        "prob_load_shedding_percentages": [6, 8, 10, 12],
    }
    ldbc_4 = {
        "datasets": ["code/dataset/ldbc/ldbc_updatestream_sf10_peaks.txt"],
        "query_label_pairs": ldbc_query_4_label_pairs,
        "size": 259200,
        "slide": 21600,
        "load_average_horizon": 0,
        "min_variation": 0.02,
        "min_size_slide_multipliers": [6, 7, 8, 9],
        "random_load_shedding_percentages": [8, 10, 12, 14],
        "prob_load_shedding_percentages": [6, 8, 10, 12],
    }
    ldbc_alternative = {
        "datasets": ["code/dataset/ldbc/ldbc_updatestream_sf10_peaks.txt"],
        "query_label_pairs": ldbc_query_alternative_label_pairs,
        "size": 259200,
        "slide": 21600,
        "load_average_horizon": 0,
        "min_variation": 0.02,
        "min_size_slide_multipliers": [6, 7, 8, 9],
        # Reuse the shedding grids selected for LDBC query 7.
        "random_load_shedding_percentages": [2, 4, 6, 8, 10],
        "prob_load_shedding_percentages": [4, 6, 8, 10, 12],
    }


    so = {
        "datasets": ["code/dataset/so/sx_stackoverflow_merged_peaks.txt"],
        "query_label_pairs": so_query_label_pairs,
        "size": 432000,
        "slide": 21600,
        "load_average_horizon": 22,
        "min_variation": 0.02,
        "min_size_slide_multipliers": [18], # 15, 16, 17, 18
        "random_load_shedding_percentages": [6, 9, 12, 15],
        "prob_load_shedding_percentages": [12, 15, 18, 21],
    }
    so_4 = { # query 3 and 4
        "datasets": ["code/dataset/so/sx_stackoverflow_merged_peaks.txt"],
        "query_label_pairs": so_query_4_label_pairs,
        "size": 216000,
        "slide": 21600,
        "load_average_horizon": 22,
        "min_variation": 0.02,
        "min_size_slide_multipliers": [7, 8, 9, 10],
        "random_load_shedding_percentages": [6, 9, 12, 15],
        "prob_load_shedding_percentages": [12, 15, 18, 21],
    }

    current_conf = ldbc_alternative

    algorithms = [11, 10, 3, 4]
    output = "sigmod/ldbc_alternatives"

    generate_config_files(
        datasets=current_conf["datasets"],
        query_label_pairs=current_conf["query_label_pairs"],
        output=output,
        algorithms=algorithms,
        size=current_conf["size"],
        slide=current_conf["slide"],
        load_average_horizon=current_conf["load_average_horizon"],
        min_variation=current_conf["min_variation"],
        min_size_slide_multipliers=current_conf["min_size_slide_multipliers"],
        prob_load_shedding_percentages=current_conf["prob_load_shedding_percentages"],
        random_load_shedding_percentages=current_conf["random_load_shedding_percentages"],
    )


if __name__ == "__main__":
    main()
