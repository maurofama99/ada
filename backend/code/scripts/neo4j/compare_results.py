import csv
from collections import defaultdict

def read_csv(file_path):
    result_set = defaultdict(int)  # Store occurrences of source-destination pairs
    same_source_destination = 0  # Count pairs where source and destination are the same
    with open(file_path, "r") as f:
        reader = csv.reader(f)
        for row in reader:
            source, destination, _ = map(int, row)
            result_set[(source, destination)] += 1
            if source == destination:
                same_source_destination += 1
    return result_set, same_source_destination

def read_csv_db(file_path):
    result_set = defaultdict(int)  # Store occurrences of source-destination pairs
    same_source_destination = 0  # Count pairs where source and destination are the same
    with open(file_path, "r") as f:
        reader = csv.reader(f)
        for row in reader:
            source, destination = map(int, row)
            result_set[(source, destination)] += 1
            if source == destination:
                same_source_destination += 1
    return result_set, same_source_destination

def compare_results(file1, file2):
    results1, same_source_dest1 = read_csv(file1)
    results2, same_source_dest2 = read_csv_db(file2)

    set1 = set(results1.keys())
    set2 = set(results2.keys())
    common_pairs = len(set1 & set2)
    total_results_file1 = len(results1)
    total_results_file2 = len(results2)
    duplicate_pairs_file1 = sum(1 for count in results1.values() if count > 1)
    duplicate_pairs_file2 = sum(1 for count in results2.values() if count > 1)

    print("Total results in first file:", total_results_file1)
    print("Total results in second file:", total_results_file2)
    print("Common source-destination pairs in both files:", common_pairs)
    print("Duplicate source-destination pairs in first file:", duplicate_pairs_file1)
    print("Duplicate source-destination pairs in second file:", duplicate_pairs_file2)
    print("Pairs where source and destination are the same in first file:", same_source_dest1)
    print("Pairs where source and destination are the same in second file:", same_source_dest2)
    #print("Source-destination pairs in first file but not in second file:", set1-set2)

    return {
        "total_results_file1": total_results_file1,
        "total_results_file2": total_results_file2,
        "common_pairs": common_pairs,
        "duplicate_pairs_file1": duplicate_pairs_file1,
        "duplicate_pairs_file2": duplicate_pairs_file2,
        "same_source_dest1": same_source_dest1,
        "same_source_dest2": same_source_dest2
    }

# Example usage
if __name__ == "__main__":
    file1 = "output_a4_S86400_s28800_q4_z0.200000_wm0.csv"  # Stream
    file2 = "db-results_500k.csv"  # DB
    compare_results(file1, file2)

