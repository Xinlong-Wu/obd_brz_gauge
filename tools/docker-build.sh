#!/usr/bin/env bash
# Thin wrapper: run idf.py inside the official ESP-IDF container.
#
# Usage:  tools/docker-build.sh <idf.py args...>
#         tools/docker-build.sh build
#         tools/docker-build.sh set-target esp32s3   # first time
#
# The image tag can be overridden with IDF_DOCKER_IMAGE. Note that macOS
# containers cannot reach USB devices: flash/monitor on the host with
# esptool / esp-idf-monitor (see docs/FLASH.md).
set -euo pipefail

IMAGE="${IDF_DOCKER_IMAGE:-espressif/idf:v5.5.3}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

TTY=""
[ -t 0 ] && TTY="-it"

exec docker run --rm $TTY -v "$ROOT":/project -w /project "$IMAGE" idf.py "$@"
