#!/bin/bash

# Paths to binaries and configs
BINARIES=("/home/ssh_user/Mauro/sgadwin_exp/CbAW4DGSP/main")
CONFIGS=("/home/ssh_user/Mauro/sgadwin_exp/CbAW4DGSP/code/scripts/parameter_selection/baseline_config.txt" "/home/ssh_user/Mauro/sgadwin_exp/CbAW4DGSP/code/scripts/parameter_selection/baseline_config_2.txt" "/home/ssh_user/Mauro/sgadwin_exp/CbAW4DGSP/code/scripts/parameter_selection/baseline_config_3.txt" "/home/ssh_user/Mauro/sgadwin_exp/CbAW4DGSP/code/scripts/parameter_selection/baseline_config_4.txt")

# Parameter ranges to explore
MIN_SCALES=(0.1)
MAX_SCALES=(0.25)
MIN_ZSCORES=(1.8)
MAX_ZSCORES=(3.5)
MIN_LIVES=(7)
MAX_LIVES=(12)

# Number of optimization calls
CALLS=30

# Python script path
SCRIPT="optimize.py"

for BIN in "${BINARIES[@]}"; do
  for CFG in "${CONFIGS[@]}"; do
    for i in "${!MIN_SCALES[@]}"; do
      MIN_SCALE=${MIN_SCALES[$i]}
      MAX_SCALE=${MAX_SCALES[$i]}
      MIN_Z=${MIN_ZSCORES[$i]}
      MAX_Z=${MAX_ZSCORES[$i]}
      MIN_L=${MIN_LIVES[$i]}
      MAX_L=${MAX_LIVES[$i]}

      echo "Running with:"
      echo "  Binary: $BIN"
      echo "  Config: $CFG"
      echo "  Scale: $MIN_SCALE–$MAX_SCALE, Z-score: $MIN_Z–$MAX_Z, Lives: $MIN_L–$MAX_L"

      python3 "$SCRIPT" \
        --binary "$BIN" \
        --base_config "$CFG" \
        --n_calls "$CALLS" \
        --min_scale "$MIN_SCALE" \
        --max_scale "$MAX_SCALE" \
        --min_zscore "$MIN_Z" \
        --max_zscore "$MAX_Z" \
        --min_lives "$MIN_L" \
        --max_lives "$MAX_L"

      echo "---------------------------------------------"
    done
  done
done
