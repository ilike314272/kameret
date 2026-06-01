#!/usr/bin/env bash
set -euo pipefail
PRESET=${1:-linux-x86-release}
cmake --preset "$PRESET" -S "$(dirname "$0")/.."
cmake --build --preset "$PRESET" -j"$(nproc)"
