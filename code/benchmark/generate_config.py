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
                            config_filepath = os.path.join("config/vldb/" + output, config_filename)
                            with open(config_filepath, 'w') as config_file:
                                config_file.write(config_content)
                            print(f"Generated {config_filepath}")

def main():
    # SO
    # query_label_pairs = [(1,[1]), (5,[2,1,3]), (7,[3,2,1]), (2,[2,1]), (10,[3,2,1]), (6,[1,2]), (3,[3,1,2]), (4,[3,1,2])]

    # LDBC
    # datasets = ["code/dataset/ldbc/social-graph12_14v4_bursted.txt"]
    # query_label_pairs = [(4, [3,0,8]), (1, [14]), (5, [3,0,8]), (2, [3,0]), (10, [9,3,0]), (7, [8,9,0])]
    # COMPLETENESS : window_slide_pairs = [(345600, 21600, 345600, 324000), (345600, 21600, 345600, 302400), (345600, 21600, 345600, 280800), (345600, 21600, 345600, 259200), (345600, 21600, 345600, 237600)]
    # COMPLETENESS (LS) :  window_slide_pairs = [(345600, 10800, 5, 1), (345600, 10800, 10, 2), (345600, 10800, 15, 3), (345600, 10800, 20, 4), (345600, 10800, 25, 5)]
    # LATENCY / TUPUT : window_slide_pairs = [(172800, 10800, 172800, 118800), (345600, 10800, 345600, 259200)]

    # HIGGS
    higgs_dataset = ["code/dataset/higgs-activity/higgs-activity_time_postprocess.txt"]
    higgs_query_label_pairs =  [(1,[1]), (5,[2,1,3]), (7,[2,3,1]), (2,[2,1]), (10,[2,3,1]), (6,[2,1]), (3,[3,2,1]), (4,[2,1,3])]
    HIGGS_LATENCY = [(3600, 600, 3600, 2700), (7200, 600, 7200, 5400), (10800, 600, 10800, 8100)]
    HIGGS_COMPLETENESS  = [(3600, 300, 3600, 2100), (3600, 300, 3600, 2400), (3600, 300, 3600, 2700), (3600, 300, 3600, 3000), (3600, 300, 3600, 3300)]
    HIGGS_LOAD_SHEDDING = [(3600, 300, 5, 1), (3600, 300, 10, 2), (3600, 300, 15, 3), (3600, 300, 20, 4), (3600, 300, 25, 5)]



    algorithms = [11]
    query_label_pairs =  higgs_query_label_pairs
    datasets = higgs_dataset
    window_slide_pairs = HIGGS_LATENCY
    output = "latency_tput/higgs"

    # Query	        LDBC	HIGGS	SO
    # 1) a*	        0	    1	    1
    # 5) ab*c	    3,0,8	2,1,3	2,1,3
    # 7) abc*	    8,9,0	2,3,1	3,2,1
    # 2) ab*	    3,0	    2,1	    2,1
    # 10) (a|b)c*	9,3,0	2,3,1	3,2,1
    # 6) a*b*	            2,1	    1,2
    # 3) ab*c*	            3,2,1	3,1,2
    # 4) (abc)+	    3,0,8	2,1,3	3,1,2

    generate_config_files(datasets, algorithms, window_slide_pairs, query_label_pairs, output)

if __name__ == "__main__":
    main()












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
    # algorithms = [1] # 1 adaptive, 0 sliding window
    #     query_label_pairs = [(1,[1]), (5,[2,1,3]), (7,[3,2,1]), (2,[2,1]), (10,[3,2,1]), (6,[1,2]), (3,[3,1,2]), (4,[3,1,2])]
    #     datasets = ["code/dataset/so/so-stream_labelled_bursted.txt"]
    #     window_slide_pairs = [(172800, 17280, 224640, 120960), (86400, 8640, 112320, 60480)]
    #     output = "so"

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
    #     window_slide_pairs = [(518400, 28800, 518400, 489600), (518400, 28800, 518400, 460800), (518400, 28800, 518400, 432000), (518400, 28800, 518400, 403200), (518400, 28800, 518400, 374400)]
