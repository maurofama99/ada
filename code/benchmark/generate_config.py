import os

def generate_config_files(output_folder, datasets, algorithms, window_slide_pairs, query_label_pairs, z_scores, watermarks):

    for dataset in datasets:
        for algorithm in algorithms:
            for size, slide in window_slide_pairs:
                for query_type, labels in query_label_pairs:
                    for zscore, lives in z_scores:
                        for watermark, ooo_strategy in watermarks:
                            config_content = (
                                f"algorithm={algorithm}\n"
                                f"input_data_path={dataset}\n"
                                f"output_base_folder={output_folder}\n"
                                f"size={size}\n"
                                f"slide={slide}\n"
                                f"query_type={query_type}\n"
                                f"labels={','.join(map(str, labels))}\n"
                                f"zscore={zscore}\n"
                                f"lives={lives}\n"
                                f"watermark={watermark}\n"
                                f"ooo_strategy={ooo_strategy}\n"
                            )
                            config_filename = f"config_a{algorithm}_S{size}_s{slide}_q{query_type}_z{zscore}_r{lives}.txt"
                            config_filepath = os.path.join("config/higgs", config_filename)
                            with open(config_filepath, 'w') as config_file:
                                config_file.write(config_content)
                            print(f"Generated {config_filepath}")

def main():
    output_folder = "code/benchmark/results_higgs/"
    datasets = ["code/dataset/higgs-activity/higgs-activity_time_postprocess.txt"]
    algorithms = [1]
    window_slide_pairs = [(129600,10800), (86400,10800), (172800,10800), (129600,21600), (86400,21600), (172800,21600), (129600,43200), (86400,43200), (172800,43200)]
    query_label_pairs = [(1, [2]), (2, [2,1,3]), (3, [3,1]), (4, [1,3]), (5, [1,2,3]), (7, [1,2,3,3]), (9, [1,2,3])]
    z_scores = [(0, 1)]
    watermarks = [(0, 1)]

    generate_config_files(output_folder, datasets, algorithms, window_slide_pairs, query_label_pairs, z_scores, watermarks)

if __name__ == "__main__":
    main()


# scp -r config ssh_user@134.214.143.99:/home/ssh_user/Mauro/sgadwin_exp/CbAW4DGSP/code/benchmark