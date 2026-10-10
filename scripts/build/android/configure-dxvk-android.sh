#!/usr/bin/env bash
# Configure the DXVK native fork for Android ARM64 using SDL3 from the APK project.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

export ANDROID_HOME="${ANDROID_HOME:-${HOME}/Library/Android/sdk}"
export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${ANDROID_HOME}/ndk/28.2.13676358}"

ANDROID_API="${ANDROID_API:-30}"
ANDROID_ABI="arm64-v8a"
SDL_AAR="${PROJECT_ROOT}/android/app/libs/SDL3-3.4.2.aar"
BUILD_ROOT="${PROJECT_ROOT}/build/android-dxvk"
SDL_ROOT="${BUILD_ROOT}/deps/sdl3"
CONFIG_ROOT="${BUILD_ROOT}/config"
MESON_BUILD_ROOT="${BUILD_ROOT}/dxvk"
DXVK_SOURCE="${PROJECT_ROOT}/references/fbraz3-dxvk"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "ERROR: Required command not found: $1" >&2
        exit 1
    fi
}

require_path() {
    if [[ ! -e "$1" ]]; then
        echo "ERROR: Missing $2: $1" >&2
        exit 1
    fi
}

require_command cmake
require_command meson
require_command pkg-config
require_command unzip
require_path "${ANDROID_NDK_ROOT}" "Android NDK r28c"

if [[ ! -f "${SDL_AAR}" ]]; then
    "${SCRIPT_DIR}/fetch-sdl3-android.sh"
fi

mkdir -p "${SDL_ROOT}" "${CONFIG_ROOT}"
unzip -oq "${SDL_AAR}" -d "${SDL_ROOT}"

cmake \
    -S "${PROJECT_ROOT}/cmake/android-dxvk" \
    -B "${CONFIG_ROOT}/cmake" \
    -DANDROID_NDK_ROOT="${ANDROID_NDK_ROOT}" \
    -DANDROID_API="${ANDROID_API}" \
    -DANDROID_ABI="${ANDROID_ABI}" \
    -DSDL3_ROOT="${SDL_ROOT}" \
    -DOUTPUT_DIRECTORY="${CONFIG_ROOT}" \
    -DPKG_CONFIG_EXECUTABLE="$(command -v pkg-config)"

MESON_ARGS=(
    setup
    "${MESON_BUILD_ROOT}"
    "${DXVK_SOURCE}"
    --cross-file "${CONFIG_ROOT}/meson-arm64-android-cross.ini"
    --buildtype release
    -Denable_dxgi=false
    -Denable_d3d10=false
    -Denable_d3d11=false
    -Ddxvk_native_wsi=sdl3
)

if [[ -f "${MESON_BUILD_ROOT}/meson-private/coredata.dat" ]]; then
    MESON_ARGS+=(--reconfigure)
fi

echo "Configuring DXVK native for Android API ${ANDROID_API} (${ANDROID_ABI})"
PKG_CONFIG_PATH="${CONFIG_ROOT}/pkgconfig" meson "${MESON_ARGS[@]}"

echo "DXVK Android build configured: ${MESON_BUILD_ROOT}"
