#!/usr/bin/env bash
# Configure, build, sign, and install GeneralsXZH on a connected iPhone/iPad.
#
# Usage:
#   ./scripts/build/ios/setup-install-ios-zh.sh [--full] [--no-install]
#       [--team TEAM_ID] [--bundle BUNDLE_ID]
#
# Defaults to a fast code-only development package. Use --full to bundle game
# assets. Signing values can also be supplied through GX_TEAM_ID and
# GX_BUNDLE_ID.
# GeneralsX @build BenderAI 12/07/2026 Add a guided physical-device workflow.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

TEAM_ID="${GX_TEAM_ID:-URX49WBQ9N}"
BUNDLE_ID="${GX_BUNDLE_ID:-com.nathanbradshaw.generalszh}"
DEV_MODE=1
DO_INSTALL=1

usage() {
    sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
}

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "Required command '$1' was not found. $2"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --full)
            DEV_MODE=0
            shift
            ;;
        --no-install)
            DO_INSTALL=0
            shift
            ;;
        --team)
            [[ $# -ge 2 ]] || fail "--team requires a Team ID"
            TEAM_ID="$2"
            shift 2
            ;;
        --bundle)
            [[ $# -ge 2 ]] || fail "--bundle requires a bundle identifier"
            BUNDLE_ID="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "Unknown argument '$1'. Run with --help for usage."
            ;;
    esac
done

echo "==> GeneralsXZH guided iOS setup"
echo "    Team:   ${TEAM_ID}"
echo "    Bundle: ${BUNDLE_ID}"
echo "    Assets: $([[ ${DEV_MODE} == 1 ]] && echo 'development package' || echo 'full package')"

require_command xcodebuild "Install Xcode from the App Store."
require_command xcodegen "Install it with: brew install xcodegen"
require_command cmake "Install it with: brew install cmake"
require_command xcrun "Select Xcode with: sudo xcode-select -s /Applications/Xcode.app"
require_command security "This script must run on macOS."

if ! xcodebuild -version >/dev/null 2>&1; then
    fail "Xcode command-line tools are not ready. Open Xcode once, accept its license, and install requested components."
fi

if ! security find-identity -v -p codesigning 2>/dev/null | grep -q 'Apple Development'; then
    echo
    echo "No valid Apple Development certificate was found."
    echo "Open Xcode -> Settings -> Accounts, sign in, then choose"
    echo "Manage Certificates -> + -> Apple Development."
    echo "After Xcode creates it, rerun this script."
    exit 2
fi

DEVICE_OUTPUT=""
if ! DEVICE_OUTPUT="$(xcrun devicectl list devices 2>&1)"; then
    echo "${DEVICE_OUTPUT}" >&2
    fail "Apple CoreDevice is unavailable. Reconnect and unlock the iPad, open Xcode's Devices window, then retry."
fi

# GeneralsX @bugfix BenderAI 12/07/2026 Newer CoreDevice versions report a
# trusted device as "available (paired)" rather than "connected".
DEVICE_ID="$(printf '%s\n' "${DEVICE_OUTPUT}" | grep -Ei 'connected|available \(paired\)' | grep -oE '[0-9A-Fa-f-]{36}' | head -1 || true)"
if [[ ${DO_INSTALL} == 1 && -z ${DEVICE_ID} ]]; then
    echo "${DEVICE_OUTPUT}"
    fail "No available paired iPhone/iPad was found. Connect by USB, tap Trust, enable Developer Mode, and keep it unlocked."
fi
if [[ -n ${DEVICE_ID} ]]; then
    echo "    Device: ${DEVICE_ID}"
fi

if [[ ! -d "${HOME}/GeneralsX/MoltenVK/MoltenVK/MoltenVK/dynamic/MoltenVK.xcframework/ios-arm64/MoltenVK.framework" && -z "${GX_MOLTENVK:-}" ]]; then
    fail "MoltenVK is not staged. Run ./scripts/build/ios/fetch-moltenvk.sh once, then retry."
fi

# GeneralsX @bugfix BenderAI 12/07/2026 CMakePresets expands VULKAN_SDK
# directly. Detect the installed LunarG platform directory so an unset shell
# variable cannot silently become /lib/MoltenVK.xcframework/....
VULKAN_SDK_ROOT="${VULKAN_SDK:-}"
if [[ -n ${VULKAN_SDK_ROOT} && -d "${VULKAN_SDK_ROOT}/macOS" ]]; then
    VULKAN_SDK_ROOT="${VULKAN_SDK_ROOT}/macOS"
fi
if [[ -z ${VULKAN_SDK_ROOT} ]]; then
    for candidate in "${HOME}"/VulkanSDK/*/macOS; do
        if [[ -f "${candidate}/lib/MoltenVK.xcframework/ios-arm64/libMoltenVK.a" ]]; then
            VULKAN_SDK_ROOT="${candidate}"
        fi
    done
fi
if [[ ! -f "${VULKAN_SDK_ROOT}/lib/MoltenVK.xcframework/ios-arm64/libMoltenVK.a" || ! -f "${VULKAN_SDK_ROOT}/include/vulkan/vulkan.h" ]]; then
    fail "The LunarG Vulkan SDK with static iOS MoltenVK was not found. Set VULKAN_SDK to its macOS directory."
fi
export VULKAN_SDK="${VULKAN_SDK_ROOT}"
echo "    Vulkan: ${VULKAN_SDK}"

cd "${PROJECT_ROOT}"

echo "==> Configuring GeneralsOnline-enabled iOS build"
cmake --preset ios-vulkan -DSAGE_GENERALS_ONLINE=ON

echo "==> Building ARM64 iOS GeneralsXZH"
cmake --build build/ios-vulkan --target z_generals -j "$(sysctl -n hw.logicalcpu)"

PACKAGE_ARGS=()
if [[ ${DEV_MODE} == 1 ]]; then
    PACKAGE_ARGS+=(--dev)
fi
if [[ ${DO_INSTALL} == 1 ]]; then
    PACKAGE_ARGS+=(--install)
fi

echo "==> Signing and packaging"
GX_TEAM_ID="${TEAM_ID}" GX_BUNDLE_ID="${BUNDLE_ID}" \
    "${SCRIPT_DIR}/package-ios-zh.sh" "${PACKAGE_ARGS[@]}"

echo
if [[ ${DO_INSTALL} == 1 ]]; then
    echo "SUCCESS: GeneralsXZH is installed on the connected device."
    echo "Launch it while the device is unlocked and accept Local Network access."
else
    echo "SUCCESS: Package created at build/ios-package/GeneralsXZH.app"
fi
