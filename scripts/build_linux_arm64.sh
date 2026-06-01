#!/usr/bin/env bash
set -euo pipefail
PRESET=${1:-linux-arm64-release}
cmake --preset "$PRESET" -S "$(dirname "$0")/.."
cmake --build --preset "$PRESET" -j"$(nproc)"
