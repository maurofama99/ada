#!/bin/bash
set -euo pipefail

# -- Debug Mode --
DEBUG="${DEBUG:-0}"

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
if [ "$DEBUG" = "1" ]; then
    build_dir="$script_dir/build-debug"
    build_type="Debug"
    echo "DEBUG MODE: Compiling with -g and AddressSanitizer..."
else
    build_dir="$script_dir/build-benchmark"
    build_type="Release"
fi

mkdir -p "$build_dir"

echo "Configuring with CMake..."
if [ "$DEBUG" = "1" ]; then
    cmake -S "$script_dir" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="-g -fsanitize=address -fno-omit-frame-pointer" \
        -DCMAKE_C_FLAGS="-g -fsanitize=address -fno-omit-frame-pointer" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
else
    cmake -S "$script_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
fi

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

    if [ "$DEBUG" = "1" ]; then
        # Enable ASAN symbolizer for readable stack traces
        export ASAN_OPTIONS="symbolize=1:detect_leaks=1:abort_on_error=1"
        export ASAN_SYMBOLIZER_PATH="$(which llvm-symbolizer 2>/dev/null || which addr2line 2>/dev/null || echo "")"

        # Run with error capture
        set +e
        error_output=$("$exe_path" "$config_file" 2>&1 | tee "$output_file")
        exit_code=$?
        set -e

        if [ $exit_code -ne 0 ]; then
            echo "   FAILED (exit code: $exit_code)"
            echo "   --- Error Details ---"
            echo "$error_output" | tail -50
            echo "   ---------------------"
        else
            echo "   Success"
        fi
    else
        "$exe_path" "$config_file" > "$output_file"
        if [ $? -eq 0 ]; then
            echo "   Success"
        else
            echo "   Failed"
        fi
    fi
done

echo "All configurations processed"