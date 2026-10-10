#!/usr/bin/env bash
# Build Zero Hour Android ARM64 dependencies; --online adds TLS, WebSockets and ICE.
# Usage: ./scripts/build/android/build-engine-deps-android.sh [--online]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

export ANDROID_HOME="${ANDROID_HOME:-${HOME}/Library/Android/sdk}"
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-${ANDROID_HOME}/ndk/28.2.13676358}"
VCPKG_ROOT="${VCPKG_ROOT:-${HOME}/vcpkg}"
VCPKG="${VCPKG_ROOT}/vcpkg"
INSTALL_ROOT="${PROJECT_ROOT}/build/android-vcpkg"

if [[ ! -x "${VCPKG}" ]]; then
    echo "ERROR: vcpkg was not found: ${VCPKG}" >&2
    exit 1
fi
if [[ ! -d "${ANDROID_NDK_HOME}" ]]; then
    echo "ERROR: Android NDK r28c was not found: ${ANDROID_NDK_HOME}" >&2
    exit 1
fi

# GeneralsX @build Codex 08/10/2026 Use the same GNS overlay as Apple clients for Android ICE interoperability.
DEPS=(ffmpeg:arm64-android freetype:arm64-android glm:arm64-android gli:arm64-android zlib:arm64-android)
if [[ "${1:-}" == "--online" && $# -eq 1 ]]; then
    DEPS+=("curl[core,openssl,websockets]:arm64-android" "gamenetworkingsockets[ice]:arm64-android")
elif [[ $# -ne 0 ]]; then
    echo "ERROR: usage: $0 [--online]" >&2
    exit 1
fi

echo "Building Zero Hour dependencies for Android ARM64"
"${VCPKG}" install --classic \
    "${DEPS[@]}" \
    --overlay-ports="${PROJECT_ROOT}/cmake/vcpkg-overlay-ports" \
    --x-install-root="${INSTALL_ROOT}"

echo "Android engine dependencies ready: ${INSTALL_ROOT}/arm64-android"
