import csv
import sys

def remove_last_x_lines(input_file, output_file, x):
    with open(input_file, 'r') as infile:
        lines = infile.readlines()

    # Print the first x lines
    lines = lines[:x]

    with open(output_file, 'w', newline='') as outfile:
        writer = csv.writer(outfile)
        for line in lines:
            writer.writerow(line.strip().split(''))

def remove_last_x_lines_txt(input_file, output_file, x, delimiter=' '):
    with open(input_file, 'r') as infile:
        lines = infile.readlines()

    # Print the first x lines
    lines = lines[:x]

    with open(output_file, 'w', newline='') as outfile:
        writer = csv.writer(outfile, delimiter=delimiter)
        for line in lines:
            writer.writerow(line.strip().split(delimiter))

if __name__ == "__main__":
    input_file = "../dataset/so/so-stream_debug_500k.txt"
    output_file = "../dataset/so/so-stream_debug_30.txt"

    remove_last_x_lines_txt(input_file, output_file, 30)