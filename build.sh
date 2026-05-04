#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=build-rmp
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release

if [[ -d "bin/rMP.app" ]]; then
	echo "Built app bundle -> $(pwd)/bin/rMP.app"
else
	echo "Built executable -> $(pwd)/bin/rMP"
fi
