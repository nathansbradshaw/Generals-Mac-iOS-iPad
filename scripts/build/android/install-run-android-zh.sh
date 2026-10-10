#!/usr/bin/env bash
# Install and launch the GeneralsXZH Android debug APK on the active device/emulator.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
ANDROID_HOME="${ANDROID_HOME:-${HOME}/Library/Android/sdk}"
ADB="${ANDROID_HOME}/platform-tools/adb"
APK="${PROJECT_ROOT}/android/app/build/outputs/apk/debug/app-debug.apk"
PACKAGE="com.nathanbradshaw.generalsxzh"

if [[ ! -x "${ADB}" ]]; then
    echo "ERROR: adb not found: ${ADB}" >&2
    exit 1
fi
if [[ ! -f "${APK}" ]]; then
    echo "ERROR: APK not found. Build first with:" >&2
    echo "  ./scripts/build/android/build-android-zh.sh" >&2
    exit 1
fi
if ! "${ADB}" get-state >/dev/null 2>&1; then
    echo "ERROR: No running Android device/emulator was found" >&2
    exit 1
fi

"${ADB}" install -r "${APK}"
"${ADB}" logcat -c
"${ADB}" shell am force-stop "${PACKAGE}"
"${ADB}" shell monkey -p "${PACKAGE}" -c android.intent.category.LAUNCHER 1 >/dev/null

echo "GeneralsXZH Android launched. Focused logs:"
echo "  ${ADB} logcat -s GeneralsXAndroid:V SDL:V AndroidRuntime:E"
