#!/usr/bin/env bash
# Build the DXVK native D3D8/D3D9 libraries for Android ARM64.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_ROOT="${PROJECT_ROOT}/build/android-dxvk/dxvk"

"${SCRIPT_DIR}/configure-dxvk-android.sh"

echo "Building DXVK D3D8 and D3D9 for Android ARM64"
ninja -C "${BUILD_ROOT}" \
    src/d3d9/libdxvk_d3d9.so \
    src/d3d8/libdxvk_d3d8.so

echo "DXVK Android libraries ready:"
echo "  ${BUILD_ROOT}/src/d3d9/libdxvk_d3d9.so"
echo "  ${BUILD_ROOT}/src/d3d8/libdxvk_d3d8.so"
