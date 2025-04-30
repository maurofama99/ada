#!/bin/bash

# Compile main.cpp with -O3 optimization
g++ -std=c++17 -O3 -Wno-c++11-extensions -Wno-c++17-extensions -Wno-deprecated -o main code/main.cpp

# Check if compilation was successful
# shellcheck disable=SC2181
if [ $? -ne 0 ]; then
    echo "Compilation failed"
    exit 1
fi

# Directory contenente i file di configurazione
config_dir="code/benchmark/config"

# Directory per i file di output
output_dir="code/benchmark/results"
mkdir -p "$output_dir"

# Esegui il programma con ogni file di configurazione nella directory e nelle sue sottodirectory
find "$config_dir" -type f -name "*.txt" | while read -r config_file; do
    echo "Running with configuration file: $config_file"

    # Estrai i parametri dal file di configurazione
    query_type=$(grep -E "^query_type=" "$config_file" | cut -d'=' -f2)
    slide=$(grep -E "^slide=" "$config_file" | cut -d'=' -f2)
    size=$(grep -E "^size=" "$config_file" | cut -d'=' -f2)
    zscore=$(grep -E "^zscore=" "$config_file" | cut -d'=' -f2)
    lives=$(grep -E "^lives=" "$config_file" | cut -d'=' -f2)

    # Crea il nome del file di output
    output_file="${output_dir}/${query_type}_${slide}_${size}_${zscore}_${lives}.txt"

    # Esegui il programma e salva l'output nel file
    ./main "$config_file" > "$output_file"
done