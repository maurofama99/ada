import os

def generate_config_files(datasets, algorithms, window_slide_pairs, query_label_pairs, output):

    for dataset in datasets:
        for algorithm in algorithms:
            for size, slide, max_size, min_size in window_slide_pairs:
                for query_type, labels in query_label_pairs:
                            config_content = (
                                f"adaptive={algorithm}\n"
                                f"input_data_path={dataset}\n"
                                f"size={size}\n"
                                f"slide={slide}\n"
                                f"query_type={query_type}\n"
                                f"labels={','.join(map(str, labels))}\n"
                                f"max_size={max_size}\n"
                                f"min_size={min_size}\n"
                            )
                            config_filename = f"config_a{algorithm}_S{size}_s{slide}_q{query_type}_M{max_size}_m{min_size}.txt"
                            config_filepath = os.path.join("config/" + output, config_filename)
                            with open(config_filepath, 'w') as config_file:
                                config_file.write(config_content)
                            print(f"Generated {config_filepath}")

def main():
    algorithms = [0] # 1 adaptive, 0 sliding window
    query_label_pairs = [(4, [3,0,8]), (1, [0]), (5, [3,0,8]), (2, [3,0]), (10, [9,3,0]), (7, [8,9,0])]
    datasets = ["code/dataset/ldbc/social-graph12_14v4_bursted.txt"]
    window_slide_pairs = [(345600, 21600, 0, 0)]
    output = "appendix/completeness/ldbc"

    # 21600, 43200, 64800, 86400, 108000, 194400, 259200, 324000, 388800, 453600, 518400, 583200, 648000, 712800, 777600, 842800, 907200, 972000, 1036800, 1108800, 1180800, 1252800, 1324800, 1396800, 1468800, 1540800, 1612800, 1684800, 1756800, 1828800, 1900800, 1972800

    # Query	        LDBC	HIGGS	SO
    # 1) a*	        0	    1	    1
    # 5) ab*c	    3,0,8	2,1,3	2,1,3
    # 7) abc*	    8,9,0	2,3,1	3,2,1
    # 2) ab*	    3,0	    2,1	    2,1
    # 10) (a|b)c*	9,3,0	2,3,1	3,2,1
    # 6) a*b*	            2,1	    1,2
    # 3) ab*c*	            3,2,1	3,1,2
    # 4) (abc)+	    3,0,8	2,1,3	3,1,2

    # higgs:
    # algorithms = [1] # 1 adaptive, 0 sliding window
    # query_label_pairs =  [(1,[1]), (5,[2,1,3]), (7,[2,3,1]), (2,[2,1]), (10,[2,3,1]), (6,[2,1]), (3,[3,2,1]), (4,[2,1,3])]
    # datasets = ["code/dataset/higgs-activity/higgs-activity_time_postprocess.txt"]
    # window_slide_pairs = [(4200, 420, 5880, 2520), (3000, 300, 4200, 1800), (1800, 180, 3360, 1440)]
    # output = "higgs"
    # [(2100, 420, 0, 0), (2940, 420, 0, 0),  (3780, 420, 0, 0), (4620, 420, 0, 0), (5460, 420, 0, 0)]
    # tput [(258300, 17280, 0, 0), (215100, 17280, 0, 0),  (172800, 17280, 0, 0), (129600, 17280, 0, 0), (86400, 17280, 0, 0)]  |
    # window_slide_pairs = [(2100, 420, 0, 0), (2940, 420, 0, 0),  (3780, 420, 0, 0), (4620, 420, 0, 0), (5460, 420, 0, 0)]

    # higgs completeness
    # algorithms = [1] # 1 adaptive, 0 sliding window
    # query_label_pairs = [(1,[1])]
    # datasets = ["code/dataset/higgs-activity/higgs-activity_time_postprocess.txt"]
    # window_slide_pairs = [(4200, 420, 4200, 3780), (4200, 420, 4200, 3360), (4200, 420, 4200, 2940), (4200, 420, 4200, 2520), (4200, 420, 4200, 2100)]
    # window_slide_pairs = [(3000, 200, 3000, 3000), (3000, 200, 3000, 2800), (3000, 200, 3000, 2600), (3000, 200, 3000, 2400), (3000, 200, 3000, 2200), (3000, 200, 3000, 2000)]
    # output = "completeness/higgs"

    # so
    # [(1,[1]), (5,[2,1,3]), (7,[3,2,1]), (2,[2,1]), (10,[3,2,1]), (6,[1,2]), (3,[3,1,2]), (4,[3,1,2])]
    # "code/dataset/so/so-stream_labelled_halved.txt"
    # window_slide_pairs = [(216000, 21600, 324000, 108000), (172800, 17280, 259200, 86400), (129600, 12960, 194400, 64800), (86400, 8640, 129600, 43200)]
    # output = "so"

    # ldbc:
    # query_label_pairs = [(4, [3,0,8]), (1, [0]), (5, [3,0,8]), (2, [3,0]), (10, [9,3,0]), (7, [8,9,0])]
    # datasets = ["code/dataset/ldbc/social-graph12_14v4_bursted.txt"]
    # window_slide_pairs = [(216000, 21600, 237600, 108000), (172800, 17280, 189880, 86400), (129600, 12960, 142560, 64800), (86400, 8640, 95040, 43200)]
    # output = "ldbc"
    # tput = [(216000, 19440, 0, 0), (172800, 19440, 0, 0),  (183600, 19440, 0, 0), (194400, 19440, 0, 0), (205200, 19440, 0, 0)]    | (194400, 19440, 216000, 172800)

    # ldbc completeness
    # algorithms = [1] # 1 adaptive, 0 sliding window
    # query_label_pairs = [(1,[0])]
    # datasets = ["code/dataset/ldbc/social-graph12_14v4_bursted.txt"]
    # window_slide_pairs = [(172800, 17280, 172800, 155520), (172800, 17280, 172800, 138240), (172800, 17280, 172800, 120960), (172800, 17280, 172800, 103680), (172800, 17280, 172800, 86400)]
    # output = "completeness/ldbc"
    #     window_slide_pairs = [(345600, 21600, 345600, 324000), (345600, 21600, 345600, 302400), (345600, 21600, 345600, 280800), (345600, 21600, 345600, 259200), (345600, 21600, 345600, 237600)]

    generate_config_files(datasets, algorithms, window_slide_pairs, query_label_pairs, output)

if __name__ == "__main__":
    main()
