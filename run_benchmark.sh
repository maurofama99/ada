#!/bin/bash

# Compile main.cpp with -O3 optimization
g++ -std=c++11 -O3 -Wno-c++11-extensions -Wno-c++17-extensions -Wno-deprecated -o main code/main.cpp

# Check if compilation was successful
if [ $? -ne 0 ]; then
    echo "Compilation failed"
    exit 1
fi

# Directory containing configuration files
config_dir="code/benchmark/config"

# Execute the program with each configuration file in the directory and its subdirectories
find "$config_dir" -type f -name "*.txt" | while read -r config_file; do
    echo "Running with configuration file: $config_file"
    ./main "$config_file" > dump.txt
done