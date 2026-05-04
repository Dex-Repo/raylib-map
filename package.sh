#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=build-rmp
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release

if [[ "$OSTYPE" == "darwin"* ]]; then
  cpack -G DragNDrop
elif [[ "$OSTYPE" == "linux"* ]]; then
  cpack -G TGZ
else
  cpack
fi

echo "Packages generated in: $(pwd)"
