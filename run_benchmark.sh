#!/bin/bash
set -euo pipefail

# -- Debug Mode --
DEBUG="${DEBUG:-0}"

# -- Script Setup: run from repo root so that input_data_path in configs resolves correctly --
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$script_dir"

# -- Input Validation --
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <config_dir>"
    exit 1
fi

config_dir="$(cd "$1" && pwd)"

if [ ! -d "$config_dir" ]; then
    echo "Error: config directory '$1' not found."
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
cmake --build "$build_dir" --config "$build_type" --target main_exe

# -- Executable Path --
exe_path="$build_dir/main_exe"
if [ ! -x "$exe_path" ]; then
    echo "Error: executable '$exe_path' not found or not executable."
    exit 1
fi

# -- Output Directories --
# main_exe writes CSVs to results/ automatically (relative to CWD = script_dir)
mkdir -p "$script_dir/results"
logs_dir="$script_dir/logs"
mkdir -p "$logs_dir"

# -- Process All Config Files (recursive) --
mapfile -t configs < <(find "$config_dir" -type f -name '*.txt' | sort)
if [ ${#configs[@]} -eq 0 ]; then
    echo "Error: no .txt config files found in '$config_dir' (recursive)."
    exit 1
fi

total=0
failed=0

for config_file in "${configs[@]}"; do
    total=$((total + 1))
    base="$(basename "$config_file" .txt)"
    log_file="$logs_dir/${base}.log"
    err_file="$logs_dir/${base}.err"

    echo "Running: $base"
    echo "   Config: $config_file"

    if [ "$DEBUG" = "1" ]; then
        export ASAN_OPTIONS="symbolize=1:detect_leaks=1:abort_on_error=1"
        export ASAN_SYMBOLIZER_PATH="$(which llvm-symbolizer 2>/dev/null || which addr2line 2>/dev/null || true)"
    fi

    set +e
    "$exe_path" "$config_file" > "$log_file" 2> "$err_file"
    exit_code=$?
    set -e

    if [ $exit_code -eq 0 ]; then
        echo "   Success  (log: logs/${base}.log)"
    else
        failed=$((failed + 1))
        echo "   FAILED (exit code: $exit_code)"
        echo "   Stdout: logs/${base}.log"
        echo "   Stderr: logs/${base}.err"
        if [ -s "$err_file" ]; then
            echo "   --- Last errors ---"
            tail -20 "$err_file"
            echo "   -------------------"
        fi
    fi
done

echo ""
echo "Done: $((total - failed))/$total succeeded."
[ $failed -eq 0 ] || exit 1
