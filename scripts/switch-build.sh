#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SWITCH_DIR="${ROOT_DIR}/switch"
OUT_DIR="${ROOT_DIR}/out/switch"

if [[ ! -f "${SWITCH_DIR}/Makefile" ]]; then
  echo "Missing ${SWITCH_DIR}/Makefile" >&2
  exit 1
fi

if [[ -z "${DEVKITPRO:-}" ]]; then
  export DEVKITPRO=/opt/devkitpro
fi

if [[ ! -f "${DEVKITPRO}/libnx/switch_rules" ]]; then
  echo "libnx switch_rules not found at ${DEVKITPRO}/libnx/switch_rules" >&2
  exit 1
fi

JOBS="${JOBS:-$(( $(nproc) / 2 ))}"

echo "[switch-build] Building bootstrap NRO with ${JOBS} jobs"
make -C "${SWITCH_DIR}" -j"${JOBS}"

mkdir -p "${OUT_DIR}"
cp -f "${SWITCH_DIR}/alephone.nro" "${OUT_DIR}/alephone.nro"
cp -f "${SWITCH_DIR}/alephone.elf" "${OUT_DIR}/alephone.elf"

echo "[switch-build] Output: ${OUT_DIR}/alephone.nro"
