#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_TAG="${IMAGE_TAG:-alephone-switch}"

podman build -t "${IMAGE_TAG}" -f "${ROOT_DIR}/containers/switch/Containerfile" "${ROOT_DIR}"

podman run --rm \
  -v "${ROOT_DIR}":/work:Z \
  -w /work \
  --userns keep-id \
  "${IMAGE_TAG}" \
  bash ./scripts/switch-build.sh
