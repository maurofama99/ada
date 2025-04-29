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
                            config_filepath = os.path.join("config", config_filename)
                            with open(config_filepath, 'w') as config_file:
                                config_file.write(config_content)
                            print(f"Generated {config_filepath}")

def main():
    output_folder = "code/output_debug/"
    datasets = ["code/dataset/ldbc/ldbc-sf10-updatestream_postprocess.txt"]
    algorithms = [1]
    window_slide_pairs = [(90000,5000), (100000,10000), (100000,5000)]
    query_label_pairs = [(1, [5])] # [(8, [9, 3]), (7, [4, 10, 5]), (4, [10, 5]), (9, [9, 3, 1]),  (1, [3]), (2, [4, 10, 5])]
    z_scores = [(0, 0)]
    watermarks = [(0, 1)]

    generate_config_files(output_folder, datasets, algorithms, window_slide_pairs, query_label_pairs, z_scores, watermarks)

if __name__ == "__main__":
    main()
# scp -r config ssh_user@134.214.143.99:/home/ssh_user/Mauro/sgadwin_exp/CbAW4DGSP/code/benchmark