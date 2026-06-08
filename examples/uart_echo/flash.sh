#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"
FLASH_OFFSET="${FLASH_OFFSET:-0x0}"
BIN_PATH="${1:-${SCRIPT_DIR}/${BUILD_DIR}/app.bin}"

esptool.py write_flash "${FLASH_OFFSET}" "${BIN_PATH}"
