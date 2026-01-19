#!/bin/bash
set -euo pipefail

# -- Script Setup: Ensure Path Consistency --
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$script_dir" || exit 1

# -- Input Validation --
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <config_dir>"
    exit 1
fi

config_dir="$1"

if [ ! -d "$config_dir" ]; then
    echo "Error: config directory '$config_dir' not found."
    exit 1
fi

# -- Compilation (CMake) --
build_dir="$script_dir/build-benchmark"
mkdir -p "$build_dir"

echo "Configuring with CMake..."
cmake -S "$script_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release

echo "Building target main_exe..."
cmake --build "$build_dir" --config Release --target main_exe

# -- Executable Path --
exe_path="$build_dir/main_exe"
if [ ! -x "$exe_path" ]; then
    echo "Error: executable '$exe_path' not found or not executable."
    exit 1
fi

# -- Output Path --
output_dir="$script_dir/code/benchmark/logs"
mkdir -p "$output_dir"

# -- Process All Config Files --
find "$config_dir" -type f -name "*.txt" | while read -r config_file; do
    echo "Running: $(basename "$config_file")"

    # Extract parameters from config file
    query_type=$(grep -E "^query_type=" "$config_file" | cut -d'=' -f2)
    slide=$(grep -E "^slide=" "$config_file" | cut -d'=' -f2)
    size=$(grep -E "^size=" "$config_file" | cut -d'=' -f2)
    zscore=$(grep -E "^max_size=" "$config_file" | cut -d'=' -f2)
    lives=$(grep -E "^adaptive=" "$config_file" | cut -d'=' -f2)

    # Generate output filename
    output_file="${output_dir}/${query_type}_${slide}_${size}_${zscore}_${lives}.txt"

    # Run program with absolute config path
    echo "   Input: $config_file"
    echo "   Output: $(basename "$output_file")"
    "$exe_path" "$config_file" > "$output_file"

    # Check exit status
    if [ $? -eq 0 ]; then
        echo "Success"
    else
        echo "Failed"
    fi
done

echo "All configurations processed"
