import csv

def convert_space_to_comma(input_file, output_file):
    with open(input_file, 'r') as infile, open(output_file, 'w', newline='') as outfile:
        reader = csv.reader(infile, delimiter=' ')
        writer = csv.writer(outfile, delimiter=',')

        for row in reader:
            writer.writerow(row)

if __name__ == "__main__":
    input_file = '../dataset/so/so-stream_debug_500k.txt'
    output_file = '../dataset/so/so-stream_debug_500k.csv'
    convert_space_to_comma(input_file, output_file)