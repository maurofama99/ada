import csv

def convert_labels(input_file, output_file, label_map):
    with open(input_file, 'r') as infile, open(output_file, 'w', newline='') as outfile:
        reader = csv.reader(infile)
        writer = csv.writer(outfile)

        for row in reader:
            source, destination, label, timestamp = row
            label = label_map.get(long long(label), label)  # Convert label using the map
            writer.writerow([source, destination, label, timestamp])

if __name__ == "__main__":
    input_file = '../dataset/so/so-stream_debug_500k.csv'
    output_file = '../dataset/so/so-stream_debug_500k_regex.csv'
    label_map = {
        1: 'a',
        2: 'b',
        3: 'c',
        # Add more mappings as needed
    }

    convert_labels(input_file, output_file, label_map)