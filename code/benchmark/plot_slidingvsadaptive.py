import os
import pandas as pd

# Define the base directory and the variants
base_dir = 'results_so'
variants = ['base', 'adaptive']

# Initialize an empty DataFrame with the specified structure
columns = ['q1', 'q2', 'q3', 'q4', 'q5', 'q7', 'q9']
index = pd.MultiIndex.from_product([['43200/21600', '86400/28800', '86400/43200'], range(1, 7)], names=['row', 'variant'])
df = pd.DataFrame(index=index, columns=columns)

# Function to extract row and column values from the file name
def extract_values(file_name):
    parts = file_name.split('_')
    row_value = parts[2][1:]  # Extract the number after 'S'
    row_value += "/" + parts[3][1:]  # Extract the number after 's'
    column_value = parts[4][1:]  # Extract the number after 'q'
    return row_value, column_value

# Function to parse the file content and calculate the value
def parse_file(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()
        resulting_paths = int(lines[0].split(': ')[1])
        execution_time = int(lines[3].split(': ')[1])
        value = resulting_paths
        return value

# Iterate through each variant directory
for variant in variants:
    variant_dir = os.path.join(base_dir, variant)
    for file_name in os.listdir(variant_dir):
        if file_name.endswith('.txt'):
            row_value, column_value = extract_values(file_name)
            file_path = os.path.join(variant_dir, file_name)
            value = parse_file(file_path)
            df.at[(row_value, variants.index(variant) + 1), f'q{column_value}'] = value

# Print the DataFrame
if __name__ == "__main__":
    print(df.to_string())