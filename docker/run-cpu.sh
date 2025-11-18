#!/usr/bin/env bash
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
    echo "docker is required to run this script." >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="${ORTEAF_CPU_IMAGE:-orteaf-dev-cpu}"

echo "[ORTEAF][CPU] Building image ${image}"
docker build -f "${repo_root}/docker/cpu/Dockerfile" -t "${image}" "${repo_root}"

cmd=(/bin/bash)
if [[ $# -gt 0 ]]; then
    cmd=("$@")
fi

echo "[ORTEAF][CPU] Launching container"
docker run --rm -it \
    -v "${repo_root}:/workspace" \
    -w /workspace \
    "${image}" \
    "${cmd[@]}"
