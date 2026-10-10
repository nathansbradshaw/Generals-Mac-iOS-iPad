#!/usr/bin/env bash
# Build the GeneralsXZH Android ARM64 debug APK.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
# GeneralsX @build Codex 08/10/2026 Enable online dependencies and NGMP together; --online implies the full engine.
FULL_ENGINE=0
ONLINE=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --full) FULL_ENGINE=1 ;;
        --online) FULL_ENGINE=1; ONLINE=1 ;;
        -h|--help) echo "Usage: $0 [--full] [--online]"; exit 0 ;;
        *) echo "ERROR: unknown argument '$1' (usage: $0 [--full] [--online])" >&2; exit 1 ;;
    esac
    shift
done

export ANDROID_HOME="${ANDROID_HOME:-${HOME}/Library/Android/sdk}"
export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME}}"
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-${ANDROID_HOME}/ndk/28.2.13676358}"
export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME}}"
if [[ -z "${JAVA_HOME:-}" ]]; then
    if [[ -x /opt/homebrew/opt/openjdk@21/bin/java ]]; then
        export JAVA_HOME=/opt/homebrew/opt/openjdk@21
    elif [[ -x "/Applications/Android Studio.app/Contents/jbr/Contents/Home/bin/java" ]]; then
        export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
    fi
fi

require_path() {
    if [[ ! -e "$1" ]]; then
        echo "ERROR: Missing $2: $1" >&2
        exit 1
    fi
}

require_path "${JAVA_HOME:-}/bin/java" "Java 21 runtime"
require_path "${ANDROID_HOME}/platforms/android-35" "Android SDK Platform 35"
require_path "${ANDROID_HOME}/build-tools/35.0.0" "Android Build Tools 35.0.0"
require_path "${ANDROID_NDK_HOME}" "Android NDK r28c"
require_path "${ANDROID_HOME}/cmake/3.31.6" "Android SDK CMake 3.31.6"

"${SCRIPT_DIR}/fetch-sdl3-android.sh"
if [[ "${FULL_ENGINE}" == "1" ]]; then
    if [[ "${ONLINE}" == "1" ]]; then
        "${SCRIPT_DIR}/build-engine-deps-android.sh" --online
    else
        "${SCRIPT_DIR}/build-engine-deps-android.sh"
    fi
fi
export ANDROID_API="${ANDROID_API:-30}"
"${SCRIPT_DIR}/build-dxvk-android.sh"
"${SCRIPT_DIR}/build-vulkan-driver-android.sh"

DXVK_BUILD_ROOT="${PROJECT_ROOT}/build/android-dxvk/dxvk"
ANDROID_JNI_ROOT="${PROJECT_ROOT}/android/app/build/generated/jniLibs/arm64-v8a"
mkdir -p "${ANDROID_JNI_ROOT}"
cp "${DXVK_BUILD_ROOT}/src/d3d9/libdxvk_d3d9.so" "${ANDROID_JNI_ROOT}/"
cp "${DXVK_BUILD_ROOT}/src/d3d8/libdxvk_d3d8.so" "${ANDROID_JNI_ROOT}/"

GRADLE_ARGS=(-p "${PROJECT_ROOT}/android")
if [[ "${FULL_ENGINE}" == "1" ]]; then
    GRADLE_ARGS+=(-PgeneralsxFullEngine=true -PgeneralsxOnline="$([[ "${ONLINE}" == "1" ]] && echo true || echo false)")
    echo "Building full GeneralsXZH Android ARM64 debug APK"
else
    echo "Building GeneralsXZH Android ARM64 bootstrap debug APK"
fi
"${PROJECT_ROOT}/android/gradlew" "${GRADLE_ARGS[@]}" :app:assembleDebug

APK="${PROJECT_ROOT}/android/app/build/outputs/apk/debug/app-debug.apk"
require_path "${APK}" "debug APK"
echo "Android APK ready: ${APK}"
