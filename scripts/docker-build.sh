#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE_NAME="${IMAGE_NAME:-esp32-rv-sdk}"
EXAMPLE_DIR="${1:-examples/blink_ws2812}"
BUILD_DIR="${BUILD_DIR:-build}"
CONTAINER_SOURCE_ROOT="${CONTAINER_SOURCE_ROOT:-/workspace}"
HOST_EXAMPLE_DIR="${REPO_ROOT}/${EXAMPLE_DIR}"
HOST_BUILD_DIR="${HOST_EXAMPLE_DIR}/${BUILD_DIR}"
CONTAINER_BUILD_DIR="${CONTAINER_SOURCE_ROOT}/${EXAMPLE_DIR}/${BUILD_DIR}"

shift || true

if [ ! -d "${HOST_EXAMPLE_DIR}" ]; then
    echo "Example directory not found: ${EXAMPLE_DIR}" >&2
    exit 1
fi

ensure_image() {
    if [ "${REBUILD_IMAGE:-0}" = "1" ] || ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
        echo "Building Docker image ${IMAGE_NAME}..."
        docker build -t "${IMAGE_NAME}" -f "${REPO_ROOT}/docker/Dockerfile" "${REPO_ROOT}"
    else
        echo "Reusing existing Docker image ${IMAGE_NAME}. Set REBUILD_IMAGE=1 to rebuild it."
    fi
}

ensure_image
rm -rf "${HOST_BUILD_DIR}"
mkdir -p "${HOST_BUILD_DIR}"

echo "Configuring ${EXAMPLE_DIR} in container..."
docker run --rm \
    -v "${REPO_ROOT}:${CONTAINER_SOURCE_ROOT}" \
    -w "${CONTAINER_SOURCE_ROOT}" \
    "${IMAGE_NAME}" \
    cmake \
    -S "${CONTAINER_SOURCE_ROOT}/${EXAMPLE_DIR}" \
    -B "${CONTAINER_BUILD_DIR}" \
    -GNinja \
    "$@"

echo "Building ${EXAMPLE_DIR} in container..."
docker run --rm \
    -v "${REPO_ROOT}:${CONTAINER_SOURCE_ROOT}" \
    -w "${CONTAINER_SOURCE_ROOT}" \
    "${IMAGE_NAME}" \
    cmake \
    --build "${CONTAINER_BUILD_DIR}"

echo "Docker build complete. Artifacts were built directly in ${EXAMPLE_DIR}/${BUILD_DIR}."
