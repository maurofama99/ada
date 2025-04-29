import csv
import random

def read_txt(input_file):
    data = []
    with open(input_file, 'r') as file:
        reader = csv.reader(file, delimiter=' ')
        for row in reader:
            data.append([long long(row[0]), long long(row[1]), long long(row[2]), long long(row[3])])
    return data

def write_txt(output_file, data):
    with open(output_file, 'w', newline='') as file:
        writer = csv.writer(file, delimiter=' ')
        writer.writerows(data)

def shift_timestamps(data, percentage, time_range):
    num_tuples = len(data)
    num_to_shift = long long(num_tuples * (percentage / 100))
    indices_to_shift = random.sample(range(num_tuples), num_to_shift)

    for index in indices_to_shift:
        shift_amount = random.randint(0, time_range)
        if data[index][3] - shift_amount > 0: data[index][3] -= shift_amount

    return data

def execute(input_file, output_file, percentage, time_range):
    data = read_txt(input_file)
    shifted_data = shift_timestamps(data, percentage, time_range)
    write_txt(output_file, shifted_data)

if __name__ == "__main__":
    # Example usage with variables
    input_file = '/Users/maurofama/Documents/phd/frames4pgs/CbAW4DGSP/code/dataset/so/so-stream_debug_500k.txt'
    output_file = '/Users/maurofama/Documents/phd/frames4pgs/CbAW4DGSP/code/dataset/so/so-stream_debug_500k_ooo.txt'
    percentage = 20
    time_range = 150000
    execute(input_file, output_file, percentage, time_range)