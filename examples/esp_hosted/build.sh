#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"
BUILD_PATH="${SCRIPT_DIR}/${BUILD_DIR}"

rm -rf "${BUILD_PATH}"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_PATH}" -GNinja "$@"
cmake --build "${BUILD_PATH}"
