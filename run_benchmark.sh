#!/bin/bash

# -- Script Setup: Ensure Path Consistency --
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$script_dir" || exit 1

# -- Compilation --
echo "Compiling main.cpp..."
g++ -std=c++17 -O3 \
    -Wno-c++11-extensions \
    -Wno-c++17-extensions \
    -Wno-deprecated \
    -o "$script_dir/main" \
    "$script_dir/code/main.cpp"

# Check compilation status
if [ $? -ne 0 ]; then
    echo "Compilation failed"
    exit 1
fi

# -- Configuration Paths --
config_dir="$script_dir/code/benchmark/config"
output_dir="$script_dir/code/benchmark/results"
mkdir -p "$output_dir"

# -- Process All Config Files --
find "$config_dir" -type f -name "*.txt" | while read -r config_file; do
    echo "Running: $(basename "$config_file")"

    # Extract parameters from config file
    query_type=$(  grep -E "^query_type=" "$config_file" | cut -d'=' -f2)
    slide=$(       grep -E "^slide="      "$config_file" | cut -d'=' -f2)
    size=$(        grep -E "^size="       "$config_file" | cut -d'=' -f2)
    zscore=$(      grep -E "^zscore="     "$config_file" | cut -d'=' -f2)
    lives=$(       grep -E "^lives="      "$config_file" | cut -d'=' -f2)

    # Generate output filename
    output_file="${output_dir}/${query_type}_${slide}_${size}_${zscore}_${lives}.txt"

    # Run program with absolute config path
    echo "   Input: $config_file"
    echo "   Output: $(basename "$output_file")"
    "$script_dir/main" "$config_file" > "$output_file"

    # Check exit status
    if [ $? -eq 0 ]; then
        echo "Success"
    else
        echo "Failed"
    fi
done

echo "All configurations processed"
