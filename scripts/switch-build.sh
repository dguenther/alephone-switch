#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/out/switch"
BUILD_DIR="${OUT_DIR}/build-autotools"
ICON_PATH="${ROOT_DIR}/switch/alephone-icon-256.jpg"

if [[ ! -f "${ROOT_DIR}/configure.ac" ]]; then
  echo "Missing ${ROOT_DIR}/configure.ac" >&2
  exit 1
fi

if [[ -z "${DEVKITPRO:-}" ]]; then
  export DEVKITPRO=/opt/devkitpro
fi

if [[ ! -f "${DEVKITPRO}/libnx/switch.specs" ]]; then
  echo "libnx switch.specs not found at ${DEVKITPRO}/libnx/switch.specs" >&2
  exit 1
fi

export PATH="${DEVKITPRO}/devkitA64/bin:${DEVKITPRO}/tools/bin:${DEVKITPRO}/portlibs/switch/bin:${PATH}"

for cmd in autoreconf aarch64-none-elf-gcc aarch64-none-elf-g++ aarch64-none-elf-pkg-config nacptool elf2nro; do
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "Missing required tool: ${cmd}" >&2
    exit 1
  fi
done

mkdir -p "${OUT_DIR}"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

NPROC="$(nproc)"
if (( NPROC > 1 )); then
  JOBS="${JOBS:-$((NPROC / 2))}"
else
  JOBS="${JOBS:-1}"
fi

ARCH_FLAGS="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
export PKG_CONFIG="aarch64-none-elf-pkg-config"
export CC="aarch64-none-elf-gcc"
export CXX="aarch64-none-elf-g++"
export AR="aarch64-none-elf-ar"
export RANLIB="aarch64-none-elf-ranlib"
export CFLAGS="${ARCH_FLAGS}"
export CXXFLAGS="${ARCH_FLAGS} -std=gnu++17"
export CPPFLAGS="-D__SWITCH__ -I${DEVKITPRO}/portlibs/switch/include -I${ROOT_DIR}/switch"
export LDFLAGS="-specs=${DEVKITPRO}/libnx/switch.specs ${ARCH_FLAGS} -L${DEVKITPRO}/portlibs/switch/lib -L${DEVKITPRO}/libnx/lib -lglad -lEGL -lglapi -ldrm_nouveau -lnx"
export BOOST_ROOT="${DEVKITPRO}/portlibs/switch"

echo "[switch-build] Regenerating autotools files"
(
  cd "${ROOT_DIR}"
  autoreconf -fi
)

echo "[switch-build] Configuring cross-build in ${BUILD_DIR}"
(
  cd "${BUILD_DIR}"
  NETWORKING_ARG="--enable-networking"
  if [[ "${SWITCH_DISABLE_NETWORKING:-0}" == "1" ]]; then
    NETWORKING_ARG="--disable-networking"
  fi

  "${ROOT_DIR}/configure" \
    --host=aarch64-none-elf \
    --build="$(gcc -dumpmachine)" \
    --with-boost="${DEVKITPRO}/portlibs/switch" \
    --with-boost-libdir="${DEVKITPRO}/portlibs/switch/lib" \
    --with-boost-filesystem=boost_filesystem \
    "${NETWORKING_ARG}" \
    --enable-opengl \
    --disable-steam \
    --without-curl \
    --without-zzip \
    --without-miniupnpc \
    --without-vpx \
    --without-matroska \
    --without-ebml \
    --without-vorbis \
    --without-vorbisenc \
    --without-libyuv \
    --without-nfd \
    --without-catch2
)

echo "[switch-build] Building Aleph One with ${JOBS} jobs"
make -C "${BUILD_DIR}" -j"${JOBS}"

ALEPHONE_ELF="${BUILD_DIR}/Source_Files/alephone"
if [[ ! -f "${ALEPHONE_ELF}" ]]; then
  echo "Expected ELF not found at ${ALEPHONE_ELF}" >&2
  exit 1
fi

if [[ ! -f "${ICON_PATH}" ]]; then
  echo "Expected icon not found at ${ICON_PATH}" >&2
  exit 1
fi

cp -f "${ALEPHONE_ELF}" "${OUT_DIR}/alephone.elf"
nacptool --create "Aleph One" "Aleph One Team" "0.1.0" "${OUT_DIR}/alephone.nacp"
elf2nro "${OUT_DIR}/alephone.elf" "${OUT_DIR}/alephone.nro" --nacp="${OUT_DIR}/alephone.nacp" --icon="${ICON_PATH}"

echo "[switch-build] Output: ${OUT_DIR}/alephone.nro"
