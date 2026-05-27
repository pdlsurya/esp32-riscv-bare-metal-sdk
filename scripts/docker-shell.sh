#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE_NAME="${IMAGE_NAME:-esp32-rv-sdk}"
CONTAINER_SOURCE_ROOT="${CONTAINER_SOURCE_ROOT:-/workspace}"
DOCKER_RUN_ARGS=(--rm)

if [ -t 0 ] && [ -t 1 ]; then
    DOCKER_RUN_ARGS+=(-it)
fi

if [ "${REBUILD_IMAGE:-0}" = "1" ] || ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
    echo "Building Docker image ${IMAGE_NAME}..."
    docker build -t "${IMAGE_NAME}" -f "${REPO_ROOT}/docker/Dockerfile" "${REPO_ROOT}"
else
    echo "Reusing existing Docker image ${IMAGE_NAME}. Set REBUILD_IMAGE=1 to rebuild it."
fi

docker run "${DOCKER_RUN_ARGS[@]}" \
    -v "${REPO_ROOT}:${CONTAINER_SOURCE_ROOT}" \
    -w "${CONTAINER_SOURCE_ROOT}" \
    "${IMAGE_NAME}"
