import matplotlib.pyplot as plt

def plot_zscore(filename):
    # Read the values from the file
    with open(filename, 'r') as file:
        values = [float(line.strip()) for line in file]

    threshold = 0.0003
    filtered_values = [value for value in values if value < threshold]

    plt.plot(filtered_values, marker='o')
    plt.title('Values Greater Than Threshold')
    plt.xlabel('Index')
    plt.ylabel('Value')
    plt.grid(True)
    plt.show()

if __name__ == "__main__":
    filename = 'dumps/density_so_15kk.txt'
    plot_zscore(filename)