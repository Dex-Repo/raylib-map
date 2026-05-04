#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=build-rmp
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug

echo "Built debug rMP -> $(pwd)/bin/rMP"
