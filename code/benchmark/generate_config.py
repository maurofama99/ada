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
                            config_filepath = os.path.join("config/ldbc", config_filename)
                            with open(config_filepath, 'w') as config_file:
                                config_file.write(config_content)
                            print(f"Generated {config_filepath}")

def main():
    output_folder = "code/benchmark/results_ldbc/"
    datasets = ["code/dataset/ldbc/social-graph12_14v2.txt"]
    algorithms = [1]
    window_slide_pairs = [(194400,21600), (259200,21600), (324000,21600), (388800,21600)] # (388800,21600), (453600, 21600), (518400, 21600), (583200, 21600) / (50544, 21600)
    query_label_pairs = [(4, [3,0,8])] # (1, [0]), (5, [3,0,8]), (2, [3,0]), (10, [9,3,0]))
    z_scores = [(0,1)]
    watermarks = [(0,1)]

    # 21600, 43200, 64800, 86400, 108000, 194400, 259200, 324000, 388800, 453600, 518400, 583200, 648000, 712800, 777600, 842800, 907200, 972000, 1036800, 1108800, 1180800, 1252800, 1324800, 1396800, 1468800, 1540800, 1612800, 1684800, 1756800, 1828800, 1900800, 1972800

    # 1:  a*,
    # 5:  ab*c,
    # 7:  abc*,
    # 11: abc,
    # 2:  ab*,
    # 10: (a|b)c*,
    # 6:  a*b*,
    # 3:  ab*c

    # higgs: [(1,[1]), (5,[2,1,3]), (7,[2,3,1]), (2,[2,1]), (10,[2,3,1]), (6,[2,1]), (3,[3,2,1]), (4,[2,1,3])]
    # ldbc: [(4, [3,0,8])] (1, [0]), (5, [3,0,8]), (2, [3,0]), (10, [9,3,0])) (7, [8,9,0])]

    generate_config_files(output_folder, datasets, algorithms, window_slide_pairs, query_label_pairs, z_scores, watermarks)

if __name__ == "__main__":
    main()


# scp -r config ssh_user@134.214.143.99:/home/ssh_user/Mauro/sgadwin_exp/CbAW4DGSP/code/benchmark